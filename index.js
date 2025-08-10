const TelegramBot = require('node-telegram-bot-api');
const sqlite3 = require('sqlite3').verbose();

// ==== CONFIG ====
const token = "NHẬP_TOKEN_CỦA_BẠN_VÀO_ĐÂY";
const bot = new TelegramBot(token, { polling: true });

// ==== DB ====
const db = new sqlite3.Database('./data.db');
db.run(`CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY,
    username TEXT,
    coins INTEGER DEFAULT 0,
    last_claim INTEGER DEFAULT 0
)`);

// ==== /start ====
bot.onText(/\/start/, (msg) => {
    const chatId = msg.chat.id;
    db.get(`SELECT * FROM users WHERE id = ?`, [chatId], (err, row) => {
        if (!row) {
            db.run(`INSERT INTO users (id, username, coins) VALUES (?, ?, 0)`,
                [chatId, msg.chat.username || ''],
                () => {
                    bot.sendMessage(chatId, "Chào mừng! Bạn đã được thêm vào hệ thống.");
                }
            );
        } else {
            bot.sendMessage(chatId, "Chào mừng trở lại!");
        }
    });
});

// ==== Lệnh nhận xu (/claim) với cooldown 1 phút ====
bot.onText(/\/claim/, (msg) => {
    const chatId = msg.chat.id;
    const now = Date.now();

    db.get(`SELECT coins, last_claim FROM users WHERE id = ?`, [chatId], (err, row) => {
        if (!row) return bot.sendMessage(chatId, "Bạn chưa /start.");
        const cooldown = 60 * 1000; // 1 phút
        if (now - row.last_claim < cooldown) {
            let waitSec = Math.ceil((cooldown - (now - row.last_claim)) / 1000);
            return bot.sendMessage(chatId, `Bạn cần chờ ${waitSec} giây nữa.`);
        }
        const newCoins = row.coins + 10;
        db.run(`UPDATE users SET coins = ?, last_claim = ? WHERE id = ?`, [newCoins, now, chatId], () => {
            bot.sendMessage(chatId, `Bạn nhận được 10 xu! Tổng: ${newCoins} xu.`);
        });
    });
});
bot.onText(/\/bonus/, (msg) => {
    const opts = {
        reply_markup: {
            inline_keyboard: [
                [{ text: "Nhận xu ngay", callback_data: "get_bonus" }]
            ]
        }
    };
    bot.sendMessage(msg.chat.id, "Bấm nút để nhận xu:", opts);
});

bot.on("callback_query", (query) => {
    const chatId = query.message.chat.id;
    if (query.data === "get_bonus") {
        db.get(`SELECT coins FROM users WHERE id = ?`, [chatId], (err, row) => {
            const newCoins = row.coins + 5;
            db.run(`UPDATE users SET coins = ? WHERE id = ?`, [newCoins, chatId], () => {
                bot.answerCallbackQuery(query.id, { text: "Bạn nhận được 5 xu!" });
                bot.editMessageText(`Bạn đã nhận xu! Tổng: ${newCoins} xu.`, {
                    chat_id: chatId,
                    message_id: query.message.message_id
                });
            });
        });
    }
});
bot.onText(/\/spin/, (msg) => {
    const chatId = msg.chat.id;
    const reward = Math.floor(Math.random() * 50) + 1;
    db.get(`SELECT coins FROM users WHERE id = ?`, [chatId], (err, row) => {
        const newCoins = row.coins + reward;
        db.run(`UPDATE users SET coins = ? WHERE id = ?`, [newCoins, chatId], () => {
            bot.sendMessage(chatId, `🎰 Bạn quay được ${reward} xu! Tổng: ${newCoins} xu.`);
        });
    });
});
db.run(`CREATE TABLE IF NOT EXISTS redeem (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,
    name TEXT,
    phone TEXT,
    email TEXT,
    coins_spent INTEGER
)`);

bot.onText(/\/redeem/, (msg) => {
    bot.sendMessage(msg.chat.id, "Nhập thông tin đổi thưởng theo dạng:\nTên | SĐT | Email | Xu cần đổi");
});

bot.on("message", (msg) => {
    if (msg.text.includes("|")) {
        const parts = msg.text.split("|").map(p => p.trim());
        if (parts.length === 4) {
            const [name, phone, email, coinsStr] = parts;
            const coins = parseInt(coinsStr);
            const chatId = msg.chat.id;

            db.get(`SELECT coins FROM users WHERE id = ?`, [chatId], (err, row) => {
                if (!row || row.coins < coins) return bot.sendMessage(chatId, "Không đủ xu.");
                db.run(`UPDATE users SET coins = ? WHERE id = ?`, [row.coins - coins, chatId]);
                db.run(`INSERT INTO redeem (user_id, name, phone, email, coins_spent) VALUES (?, ?, ?, ?, ?)`,
                    [chatId, name, phone, email, coins]
                );
                bot.sendMessage(chatId, "Yêu cầu đổi thưởng đã được ghi nhận!");
            });
        }
    }
});
