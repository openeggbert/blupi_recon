package io.github.missingno7.speedyblupi;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiManager;

import org.libsdl.app.SDLActivity;

// The game: SDL's activity running libmain.so (port/CMakeLists.txt). The game
// data is already in app storage (SetupActivity); the native side finds it
// through SDL_GetAndroidInternalStoragePath() (platform/host.cpp data_dirs).
//
// Development: the game's command-line options can be passed as one string,
//   adb shell am start -n io.github.missingno7.speedyblupi/.SetupActivity --es args "--verbose"
public class BlupiActivity extends SDLActivity {
    // Many devices drop broadcast packets in Wi-Fi power save: while the game is in the foreground it
    // holds a multicast lock, so LAN discovery queries reach a phone that hosts a session.
    private WifiManager.MulticastLock multicastLock;

    @Override
    protected void onResume() {
        super.onResume();
        WifiManager wifi = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        if (wifi != null && multicastLock == null) {
            multicastLock = wifi.createMulticastLock("speedyblupi-lan-discovery");
            multicastLock.setReferenceCounted(false);
            multicastLock.acquire();
        }
    }

    @Override
    protected void onPause() {
        if (multicastLock != null) {
            multicastLock.release();
            multicastLock = null;
        }
        super.onPause();
    }

    // The server screen's click on a new room's ID (port/common/ui/address_screen.cpp, through
    // platform/host.cpp share_room): the system's share sheet with the ID, so that a player can send
    // it in a chat. Called by JNI from the game's thread.
    public void shareText(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent send = new Intent(Intent.ACTION_SEND);
                send.setType("text/plain");
                send.putExtra(Intent.EXTRA_TEXT, text);
                try {
                    startActivity(Intent.createChooser(send, null));
                } catch (ActivityNotFoundException e) {
                    // No app to share with: the ID is on the clipboard as well.
                }
            }
        });
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    @Override
    protected String[] getArguments() {
        String args = getIntent() != null ? getIntent().getStringExtra("args") : null;
        if (args == null || args.trim().isEmpty()) return new String[0];
        return args.trim().split("\\s+");
    }
}
