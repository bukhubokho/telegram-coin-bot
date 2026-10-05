const sqlite3    = require('sqlite3').verbose();
const express    = require('express');
const cors       = require('cors');
const crypto     = require('crypto');

// ==== CONFIG ====
const API_PORT       = process.env.PORT || 3000;
const ADMIN_SECRET   = process.env.ADMIN_SECRET || "benzex-admin-secret-changeme";
let   maintenanceMode = process.env.MAINTENANCE === '1';

// ==== DB ====
const DB_PATH = process.env.DB_PATH || './data.db';
const db = new sqlite3.Database(DB_PATH);

db.serialize(() => {
    db.run(`CREATE TABLE IF NOT EXISTS bx_products (
        id TEXT PRIMARY KEY,
        name TEXT NOT NULL,
        category TEXT DEFAULT '',
        status TEXT DEFAULT 'Active',
        description TEXT DEFAULT '',
        durations TEXT DEFAULT '[]',
        created_at INTEGER DEFAULT (strftime('%s','now'))
    )`);

    db.run(`CREATE TABLE IF NOT EXISTS bx_resellers (
        email TEXT PRIMARY KEY,
        password TEXT NOT NULL,
        name TEXT DEFAULT '',
        expiry_date TEXT DEFAULT '',
        banned INTEGER DEFAULT 0,
        created_at INTEGER DEFAULT (strftime('%s','now'))
    )`);

    db.run(`CREATE TABLE IF NOT EXISTS bx_reseller_products (
        reseller_email TEXT,
        product_id TEXT,
        PRIMARY KEY (reseller_email, product_id)
    )`);

    db.run(`CREATE TABLE IF NOT EXISTS bx_keys (
        id TEXT PRIMARY KEY,
        key_code TEXT UNIQUE NOT NULL,
        product_id TEXT,
        product_name TEXT DEFAULT '',
        duration TEXT DEFAULT 'Lifetime',
        max_devices INTEGER DEFAULT 1,
        created_by TEXT DEFAULT '',
        prefix TEXT DEFAULT '',
        note TEXT DEFAULT '',
        status TEXT DEFAULT 'Available',
        activated_at TEXT DEFAULT NULL,
        expires_at TEXT DEFAULT NULL,
        paused_at INTEGER DEFAULT NULL,
        created_at INTEGER DEFAULT (strftime('%s','now'))
    )`);

    db.run(`CREATE TABLE IF NOT EXISTS bx_key_devices (
        key_id TEXT,
        hwid TEXT,
        activated_at INTEGER DEFAULT (strftime('%s','now')),
        PRIMARY KEY (key_id, hwid)
    )`);

    db.run(`CREATE TABLE IF NOT EXISTS bx_banned_devices (
        hwid TEXT PRIMARY KEY,
        banned_by TEXT DEFAULT '',
        reason TEXT DEFAULT '',
        banned_at INTEGER DEFAULT (strftime('%s','now'))
    )`);
});

// ── DB helpers ────────────────────────────────────────────────────────
const dbGet = (sql, params) => new Promise((res, rej) =>
    db.get(sql, params, (e, row) => e ? rej(e) : res(row)));
const dbAll = (sql, params) => new Promise((res, rej) =>
    db.all(sql, params, (e, rows) => e ? rej(e) : res(rows)));
const dbRun = (sql, params) => new Promise((res, rej) =>
    db.run(sql, params, function(e) { e ? rej(e) : res(this); }));

// ── Compute key status ────────────────────────────────────────────────
function computeKeyStatus(k) {
    if (!k.activated_at)  return 'Available';
    if (k.paused_at)      return 'Paused';
    if (k.expires_at && new Date() > new Date(k.expires_at)) return 'Expired';
    return 'Activated';
}

// ==== EXPRESS API ====
const app = express();
app.use(cors());
app.use(express.json());

// ── Auth middleware ────────────────────────────────────────────────────
function adminOnly(req, res, next) {
    const secret = req.headers['x-admin-secret'] || req.query.secret;
    if (secret !== ADMIN_SECRET) return res.status(401).json({ message: 'Unauthorized' });
    next();
}

// ─────────────────────────────────────────────────────────────────────
// PUBLIC: POST /api/activate
// Body: { key: string, hwid: string }
// ─────────────────────────────────────────────────────────────────────
app.post('/api/activate', async (req, res) => {
    try {
        const { key, hwid } = req.body || {};
        if (!key || !hwid) return res.status(400).json({ message: 'key và hwid là bắt buộc.' });

        if (maintenanceMode)
            return res.status(503).json({ message: 'Server đang bảo trì. Vui lòng thử lại sau.' });

        const banned = await dbGet('SELECT hwid FROM bx_banned_devices WHERE hwid = ?', [hwid]);
        if (banned) return res.status(403).json({ message: 'Thiết bị của bạn đã bị khoá.' });

        const k = await dbGet('SELECT * FROM bx_keys WHERE key_code = ?', [key.trim().toUpperCase()]);
        if (!k) return res.status(403).json({ message: 'Key không tồn tại.' });

        const status = computeKeyStatus(k);

        if (k.product_id) {
            const prod = await dbGet('SELECT status FROM bx_products WHERE id = ?', [k.product_id]);
            if (prod && prod.status === 'Hidden')
                return res.status(503).json({ message: 'Sản phẩm đang bảo trì. Vui lòng thử lại sau.' });
        }

        if (status === 'Expired')  return res.status(403).json({ message: 'Key đã hết hạn.' });
        if (status === 'Paused')   return res.status(503).json({ message: 'Key đang tạm dừng (bảo trì).' });

        if (status === 'Activated') {
            const dev = await dbGet(
                'SELECT hwid FROM bx_key_devices WHERE key_id = ? AND hwid = ?', [k.id, hwid]);
            if (!dev) {
                const devCount = await dbGet(
                    'SELECT COUNT(*) as cnt FROM bx_key_devices WHERE key_id = ?', [k.id]);
                if (devCount.cnt >= k.max_devices)
                    return res.status(403).json({ message: `Đã đạt giới hạn ${k.max_devices} thiết bị.` });
                await dbRun(
                    'INSERT OR IGNORE INTO bx_key_devices (key_id, hwid) VALUES (?, ?)', [k.id, hwid]);
            }
            const devices = await dbAll('SELECT hwid FROM bx_key_devices WHERE key_id = ?', [k.id]);
            return res.json({
                product:     k.product_name || '',
                expiresAt:   k.expires_at   || 'Lifetime',
                devicesUsed: devices.length,
                devicesMax:  k.max_devices
            });
        }

        // Available → activate now
        const now = new Date();
        let expiresAt = null;
        if (k.duration && k.duration !== 'Lifetime') {
            const match = k.duration.match(/^(\d+)\s*(Day|Days|Month|Months|Year|Years)$/i);
            if (match) {
                const n = parseInt(match[1]);
                const unit = match[2].toLowerCase();
                const exp = new Date(now);
                if (unit.startsWith('day'))   exp.setDate(exp.getDate() + n);
                if (unit.startsWith('month')) exp.setMonth(exp.getMonth() + n);
                if (unit.startsWith('year'))  exp.setFullYear(exp.getFullYear() + n);
                expiresAt = exp.toISOString();
            }
        }

        await dbRun(
            `UPDATE bx_keys SET status='Activated', activated_at=?, expires_at=? WHERE id=?`,
            [now.toISOString(), expiresAt, k.id]);
        await dbRun(
            'INSERT OR IGNORE INTO bx_key_devices (key_id, hwid) VALUES (?, ?)', [k.id, hwid]);

        return res.json({
            product:     k.product_name || '',
            expiresAt:   expiresAt      || 'Lifetime',
            devicesUsed: 1,
            devicesMax:  k.max_devices
        });

    } catch (err) {
        console.error('/api/activate error:', err);
        res.status(500).json({ message: 'Lỗi server.' });
    }
});

// PUBLIC: GET /api/key-info?key=XXX
app.get('/api/key-info', async (req, res) => {
    try {
        const { key } = req.query;
        if (!key) return res.status(400).json({ message: 'key required' });
        if (maintenanceMode)
            return res.status(503).json({ message: 'Server đang bảo trì.' });
        const k = await dbGet('SELECT * FROM bx_keys WHERE key_code = ?', [key.trim().toUpperCase()]);
        if (!k) return res.status(404).json({ message: 'Key không tồn tại.' });
        const devices = await dbAll('SELECT hwid FROM bx_key_devices WHERE key_id = ?', [k.id]);
        res.json({
            status:      computeKeyStatus(k),
            product:     k.product_name || '',
            duration:    k.duration     || 'Lifetime',
            expiresAt:   k.expires_at   || null,
            devicesUsed: devices.length,
            devicesMax:  k.max_devices
        });
    } catch (err) {
        res.status(500).json({ message: 'Lỗi server.' });
    }
});

// ─────────────────────────────────────────────────────────────────────
// ADMIN: Keys
// ─────────────────────────────────────────────────────────────────────
app.get('/api/admin/keys', adminOnly, async (req, res) => {
    const rows = await dbAll(
        `SELECT k.*, COUNT(d.hwid) as device_count
         FROM bx_keys k LEFT JOIN bx_key_devices d ON k.id = d.key_id
         GROUP BY k.id ORDER BY k.created_at DESC`, []);
    res.json(rows);
});

app.post('/api/admin/keys', adminOnly, async (req, res) => {
    try {
        const { productId, productName, duration, maxDevices, count, prefix, createdBy, note } = req.body;
        const n = Math.min(parseInt(count) || 1, 500);
        const results = [];
        for (let i = 0; i < n; i++) {
            const id  = Date.now().toString(36) + Math.random().toString(36).slice(2, 6);
            const pfx = (prefix || 'KEY').toUpperCase();
            const rand = crypto.randomBytes(6).toString('hex').toUpperCase();
            const keyCode = `${pfx}-${rand.slice(0,5)}-${rand.slice(5,11)}`.toUpperCase();
            await dbRun(
                `INSERT INTO bx_keys (id, key_code, product_id, product_name, duration, max_devices, created_by, prefix, note)
                 VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
                [id, keyCode, productId || '', productName || '', duration || 'Lifetime',
                 maxDevices || 1, createdBy || 'admin', pfx, note || '']
            );
            results.push({ id, keyCode });
        }
        res.json({ created: results.length, keys: results });
    } catch (err) {
        res.status(500).json({ message: err.message });
    }
});

app.put('/api/admin/keys/:id', adminOnly, async (req, res) => {
    const { duration, maxDevices } = req.body;
    await dbRun(
        `UPDATE bx_keys SET duration=?, max_devices=? WHERE id=?`,
        [duration, maxDevices || 1, req.params.id]);
    res.json({ ok: true });
});

app.delete('/api/admin/keys/:id', adminOnly, async (req, res) => {
    await dbRun('DELETE FROM bx_keys WHERE id = ?', [req.params.id]);
    await dbRun('DELETE FROM bx_key_devices WHERE key_id = ?', [req.params.id]);
    res.json({ ok: true });
});

app.get('/api/admin/keys/:id/devices', adminOnly, async (req, res) => {
    const rows = await dbAll('SELECT hwid FROM bx_key_devices WHERE key_id = ?', [req.params.id]);
    res.json(rows.map(r => r.hwid));
});

app.delete('/api/admin/keys/:id/devices', adminOnly, async (req, res) => {
    await dbRun('DELETE FROM bx_key_devices WHERE key_id = ?', [req.params.id]);
    await dbRun(`UPDATE bx_keys SET status='Available', activated_at=NULL, expires_at=NULL, paused_at=NULL WHERE id=?`, [req.params.id]);
    res.json({ ok: true });
});

app.delete('/api/admin/keys/:id/devices/:hwid', adminOnly, async (req, res) => {
    await dbRun('DELETE FROM bx_key_devices WHERE key_id = ? AND hwid = ?', [req.params.id, req.params.hwid]);
    res.json({ ok: true });
});

app.post('/api/admin/ban-device', adminOnly, async (req, res) => {
    const { hwid, bannedBy, reason } = req.body;
    await dbRun(
        'INSERT OR REPLACE INTO bx_banned_devices (hwid, banned_by, reason) VALUES (?, ?, ?)',
        [hwid, bannedBy || 'admin', reason || '']);
    res.json({ ok: true });
});

app.delete('/api/admin/ban-device/:hwid', adminOnly, async (req, res) => {
    await dbRun('DELETE FROM bx_banned_devices WHERE hwid = ?', [req.params.hwid]);
    res.json({ ok: true });
});

app.get('/api/admin/banned-devices', adminOnly, async (req, res) => {
    const rows = await dbAll('SELECT hwid FROM bx_banned_devices', []);
    res.json(rows.map(r => r.hwid));
});

// ─────────────────────────────────────────────────────────────────────
// ADMIN: Products
// ─────────────────────────────────────────────────────────────────────
app.get('/api/admin/products', adminOnly, async (req, res) => {
    const rows = await dbAll('SELECT * FROM bx_products ORDER BY created_at DESC', []);
    res.json(rows.map(r => ({ ...r, durations: JSON.parse(r.durations || '[]') })));
});

app.post('/api/admin/products', adminOnly, async (req, res) => {
    try {
        const { name, category, status, description, durations } = req.body;
        const id = Date.now().toString(36) + Math.random().toString(36).slice(2);
        await dbRun(
            `INSERT INTO bx_products (id, name, category, status, description, durations)
             VALUES (?, ?, ?, ?, ?, ?)`,
            [id, name, category || '', status || 'Active', description || '',
             JSON.stringify(durations || [])]
        );
        res.json({ id });
    } catch (err) {
        res.status(500).json({ message: err.message });
    }
});

app.put('/api/admin/products/:id', adminOnly, async (req, res) => {
    const { name, category, status, description, durations } = req.body;
    const prev = await dbGet('SELECT status FROM bx_products WHERE id = ?', [req.params.id]);
    await dbRun(
        `UPDATE bx_products SET name=?, category=?, status=?, description=?, durations=? WHERE id=?`,
        [name, category || '', status || 'Active', description || '',
         JSON.stringify(durations || []), req.params.id]
    );
    if (prev && prev.status !== 'Hidden' && status === 'Hidden') {
        await dbRun(
            `UPDATE bx_keys SET paused_at=? WHERE product_id=? AND activated_at IS NOT NULL AND paused_at IS NULL`,
            [Date.now(), req.params.id]);
    } else if (prev && prev.status === 'Hidden' && status !== 'Hidden') {
        const keys = await dbAll(
            `SELECT id, expires_at, paused_at FROM bx_keys WHERE product_id=? AND paused_at IS NOT NULL`,
            [req.params.id]);
        const now = Date.now();
        for (const k of keys) {
            const elapsed = now - k.paused_at;
            let newExpires = k.expires_at;
            if (k.expires_at) {
                newExpires = new Date(new Date(k.expires_at).getTime() + elapsed).toISOString();
            }
            await dbRun(`UPDATE bx_keys SET paused_at=NULL, expires_at=? WHERE id=?`, [newExpires, k.id]);
        }
    }
    res.json({ ok: true });
});

app.delete('/api/admin/products/:id', adminOnly, async (req, res) => {
    await dbRun('DELETE FROM bx_products WHERE id = ?', [req.params.id]);
    await dbRun('DELETE FROM bx_reseller_products WHERE product_id = ?', [req.params.id]);
    res.json({ ok: true });
});

// ─────────────────────────────────────────────────────────────────────
// ADMIN: Resellers
// ─────────────────────────────────────────────────────────────────────
app.get('/api/admin/resellers', adminOnly, async (req, res) => {
    const rows = await dbAll('SELECT * FROM bx_resellers ORDER BY created_at DESC', []);
    const result = [];
    for (const r of rows) {
        const products = await dbAll(
            'SELECT product_id FROM bx_reseller_products WHERE reseller_email = ?', [r.email]);
        result.push({ ...r, products: products.map(p => p.product_id) });
    }
    res.json(result);
});

app.post('/api/admin/resellers', adminOnly, async (req, res) => {
    try {
        const { email, password, name, expiryDate } = req.body;
        await dbRun(
            `INSERT INTO bx_resellers (email, password, name, expiry_date) VALUES (?, ?, ?, ?)`,
            [email, password, name || '', expiryDate || '']
        );
        res.json({ ok: true });
    } catch (err) {
        res.status(409).json({ message: 'Email đã tồn tại.' });
    }
});

app.put('/api/admin/resellers/:email', adminOnly, async (req, res) => {
    const { password, name, expiryDate, banned, products } = req.body;
    await dbRun(
        `UPDATE bx_resellers SET password=?, name=?, expiry_date=?, banned=? WHERE email=?`,
        [password, name || '', expiryDate || '', banned ? 1 : 0, req.params.email]
    );
    if (Array.isArray(products)) {
        await dbRun('DELETE FROM bx_reseller_products WHERE reseller_email = ?', [req.params.email]);
        for (const pid of products) {
            await dbRun(
                'INSERT OR IGNORE INTO bx_reseller_products (reseller_email, product_id) VALUES (?, ?)',
                [req.params.email, pid]);
        }
    }
    res.json({ ok: true });
});

app.delete('/api/admin/resellers/:email', adminOnly, async (req, res) => {
    await dbRun('DELETE FROM bx_resellers WHERE email = ?', [req.params.email]);
    await dbRun('DELETE FROM bx_reseller_products WHERE reseller_email = ?', [req.params.email]);
    res.json({ ok: true });
});

// ─────────────────────────────────────────────────────────────────────
// ADMIN: Maintenance
// ─────────────────────────────────────────────────────────────────────
app.post('/api/admin/maintenance', adminOnly, async (req, res) => {
    const { on } = req.body;
    maintenanceMode = !!on;
    if (maintenanceMode) {
        await dbRun(
            `UPDATE bx_keys SET paused_at=? WHERE activated_at IS NOT NULL AND paused_at IS NULL`,
            [Date.now()]);
    } else {
        const keys = await dbAll(
            `SELECT id, expires_at, paused_at FROM bx_keys WHERE paused_at IS NOT NULL`, []);
        const now = Date.now();
        for (const k of keys) {
            const elapsed = now - k.paused_at;
            let newExpires = k.expires_at;
            if (k.expires_at) {
                newExpires = new Date(new Date(k.expires_at).getTime() + elapsed).toISOString();
            }
            await dbRun(`UPDATE bx_keys SET paused_at=NULL, expires_at=? WHERE id=?`, [newExpires, k.id]);
        }
    }
    res.json({ maintenance: maintenanceMode });
});

app.get('/api/admin/maintenance', adminOnly, (req, res) => {
    res.json({ maintenance: maintenanceMode });
});

// RESELLER LOGIN
app.post('/api/reseller/login', async (req, res) => {
    try {
        const { email, password } = req.body;
        if (maintenanceMode)
            return res.status(503).json({ message: 'Panel đang bảo trì.' });
        const r = await dbGet('SELECT * FROM bx_resellers WHERE email = ?', [email]);
        if (!r || r.password !== password)
            return res.status(401).json({ message: 'Sai email hoặc mật khẩu.' });
        if (r.banned)
            return res.status(403).json({ message: 'Tài khoản đã bị khoá.' });
        if (r.expiry_date && new Date() > new Date(r.expiry_date))
            return res.status(403).json({ message: 'Tài khoản đã hết hạn.' });
        const products = await dbAll(
            'SELECT product_id FROM bx_reseller_products WHERE reseller_email = ?', [email]);
        res.json({
            email:      r.email,
            name:       r.name,
            expiryDate: r.expiry_date,
            products:   products.map(p => p.product_id)
        });
    } catch (err) {
        res.status(500).json({ message: 'Lỗi server.' });
    }
});

// Health check
app.get('/api/ping', (req, res) => res.json({ ok: true, maintenance: maintenanceMode }));

app.listen(API_PORT, () => {
    console.log(`[BENZ EX API] Running on port ${API_PORT}`);
});
