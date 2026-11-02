package com.schuurman.idlemaintenance;

import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.PowerManager;
import android.os.UserHandle;
import android.util.Log;

/**
 * Sends ActivityManagerService.ACTION_TRIGGER_IDLE once the screen has been off for DELAY_MS. CarIdlenessTracker then reports the device idle
 * until the next screen-on, so idle-constrained jobs (Play auto-updates / garage mode installs, etc.) can run.
 */
public class IdleMaintenanceApp extends Application {
    private static final String TAG = "IdleMaintenance";
    private static final String ACTION_TRIGGER_IDLE = "com.android.server.ACTION_TRIGGER_IDLE";
    private static final long DELAY_MS = 10 * 60_000;

    private Handler mHandler;
    private final Runnable mTrigger = this::triggerIdle;

    @Override
    public void onCreate() {
        super.onCreate();
        HandlerThread t = new HandlerThread(TAG);
        t.start();
        mHandler = new Handler(t.getLooper());

        IntentFilter f = new IntentFilter();
        f.addAction(Intent.ACTION_SCREEN_OFF);
        f.addAction(Intent.ACTION_SCREEN_ON);
        registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context c, Intent i) {
                if (Intent.ACTION_SCREEN_OFF.equals(i.getAction())) {
                    schedule();
                } else {
                    mHandler.removeCallbacks(mTrigger);
                }
            }
        }, f, null, mHandler, Context.RECEIVER_NOT_EXPORTED);

        // Booted (or restarted) with the screen already off: no SCREEN_OFF will come.
        mHandler.post(() -> {
            if (!getSystemService(PowerManager.class).isInteractive()) schedule();
        });
    }

    private void schedule() {
        mHandler.removeCallbacks(mTrigger);
        mHandler.postDelayed(mTrigger, DELAY_MS);
    }

    private void triggerIdle() {
        if (getSystemService(PowerManager.class).isInteractive()) return;
        Log.i(TAG, "screen off, triggering idle maintenance");
        sendBroadcastAsUser(new Intent(ACTION_TRIGGER_IDLE)
                .addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY), UserHandle.SYSTEM);
    }
}
