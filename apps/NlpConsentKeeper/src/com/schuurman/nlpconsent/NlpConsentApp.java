package com.schuurman.nlpconsent;

import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;
import android.util.SparseArray;

import java.util.ArrayDeque;
import java.util.Collection;

/**
 * Re-asserts network_location_opt_in=1 in GSF's partner table for every unlocked user whenever GMS resets it.
 * Triggers: process start (persistent, boot), ACTION_USER_UNLOCKED, GMS's InjectedSettingChanged broadcast (sent
 * right after each consent write) and a ContentObserver on the partner table.
 */
public class NlpConsentApp extends Application {
    private static final String TAG = "NlpConsentKeeper";
    private static final Uri PARTNER = Uri.parse("content://com.google.settings/partner");
    private static final String KEY = "network_location_opt_in";
    private static final String GSF_AUTHORITY = "com.google.settings";
    // GMS writes the reset asynchronously; give it a moment so our write lands after it.
    private static final long SETTLE_MS = 2000;
    // Loop guard: at most MAX_WRITES per user within WINDOW_MS, then back off.
    private static final int MAX_WRITES = 5;
    private static final long WINDOW_MS = 60_000;
    private static final long BACKOFF_MS = 5 * 60_000;

    private Handler mHandler;
    private final SparseArray<ArrayDeque<Long>> mWrites = new SparseArray<>();
    private final SparseArray<Object> mTokens = new SparseArray<>();

    private Object token(int userId) {
        Object t = mTokens.get(userId);
        if (t == null) mTokens.put(userId, t = new Object());
        return t;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        HandlerThread t = new HandlerThread(TAG);
        t.start();
        mHandler = new Handler(t.getLooper());

        IntentFilter f = new IntentFilter();
        f.addAction(Intent.ACTION_USER_UNLOCKED);
        f.addAction("android.location.InjectedSettingChanged");
        registerReceiverForAllUsers(new BroadcastReceiver() {
            @Override
            public void onReceive(Context c, Intent i) {
                int user = Intent.ACTION_USER_UNLOCKED.equals(i.getAction())
                        ? i.getIntExtra(Intent.EXTRA_USER_HANDLE, getSendingUserId())
                        : getSendingUserId();
                schedule(user);
            }
        }, f, null, mHandler, Context.RECEIVER_EXPORTED);

        getContentResolver().registerContentObserverAsUser(PARTNER, true,
                new ContentObserver(mHandler) {
                    @Override
                    public void onChange(boolean self, Collection<Uri> uris, int flags, UserHandle user) {
                        schedule(user.getIdentifier());
                    }
                }, UserHandle.ALL);

        mHandler.post(() -> {
            UserManager um = getSystemService(UserManager.class);
            for (UserHandle u : um.getUserHandles(true)) schedule(u.getIdentifier());
        });
    }

    private void schedule(int userId) {
        if (userId < 0) return;
        mHandler.post(() -> {
            mHandler.removeCallbacksAndMessages(token(userId));
            mHandler.postDelayed(() -> enforce(userId), token(userId), SETTLE_MS);
        });
    }

    private void enforce(int userId) {
        UserHandle user = UserHandle.of(userId);
        if (!getSystemService(UserManager.class).isUserUnlocked(user)) return;
        ContentResolver cr;
        try {
            cr = createContextAsUser(user, 0).getContentResolver();
        } catch (RuntimeException e) {
            return;
        }
        // GSF may not be installed/running for this user (yet).
        try (ContentProviderClient gsf = cr.acquireUnstableContentProviderClient(GSF_AUTHORITY)) {
            if (gsf == null) return;
        }

        String value = null;
        try (Cursor c = cr.query(PARTNER, new String[] {"value"}, "name=?", new String[] {KEY}, null)) {
            if (c != null && c.moveToFirst()) value = c.getString(0);
        } catch (RuntimeException e) {
            Log.w(TAG, "u" + userId + ": query failed: " + e);
            return;
        }
        if ("1".equals(value)) return;

        long now = System.currentTimeMillis();
        ArrayDeque<Long> w = mWrites.get(userId);
        if (w == null) mWrites.put(userId, w = new ArrayDeque<>());
        while (!w.isEmpty() && now - w.peekFirst() > WINDOW_MS) w.pollFirst();
        if (w.size() >= MAX_WRITES) {
            Log.w(TAG, "u" + userId + ": consent reset " + w.size() + "x within " + WINDOW_MS
                    + " ms, backing off");
            mHandler.postDelayed(() -> enforce(userId), token(userId), BACKOFF_MS);
            return;
        }
        w.addLast(now);

        ContentValues cv = new ContentValues();
        cv.put("name", KEY);
        cv.put("value", "1");
        try {
            cr.insert(PARTNER, cv);
            Log.i(TAG, "u" + userId + ": " + KEY + " " + value + " -> 1");
        } catch (RuntimeException e) {
            Log.w(TAG, "u" + userId + ": write failed: " + e);
        }
    }
}
