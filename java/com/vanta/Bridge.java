package com.vanta;

import android.view.MotionEvent;

public class Bridge {
    static {
        System.loadLibrary("vanta_menu");
    }

    public static native void nativeTouch(float x, float y, int action);

    public static void onTouch(MotionEvent e) {
        int action = e.getActionMasked(); // 0=DOWN 1=UP 2=MOVE
        nativeTouch(e.getX(), e.getY(), action);
    }
}
