package io.github.missingno7.speedyblupi;

import java.io.File;

// Which game a folder holds, with the native launcher's recognition rule
// (common/app/launcher.cpp RecognizesData; games/sb1/game/main.cpp): DATA/CONFIG.DEF,
// IMAGE08/INIT.BLP and the edition marker, any case. Speedy Blupi 1 and Speedy Eggbert 1
// carry DATA/DEMO200.BLP (libmain.so); Speedy Blupi 2 and Speedy Eggbert 2 carry DATA/DEMO300.BLP
// (libblupi2.so, games/sb2/game/main.cpp). The editions with programs of their own
// (port/docs/EDITIONS.md) are told apart by their installed files: the Speedy Eggbert Demo by its game
// program WCSUP.DLL (libeggbert_demo.so), Speedy Eggbert SE Valueware by its
// SpeedyEggbertSEValueware*.exe (libeggbert_value.so), and the June 1998 Speedy Blupi Demo by its one
// IMAGE folder, IMAGE/INIT.BLP without IMAGE08 (libblupi_demo.so). No Android dependency, so the
// rule is tested on the desktop (port/tests/android_game_folder.cmake).
final class GameFolder {
    // The games before INCOMPLETE and NONE (explain() picks the lowest).
    enum Kind { SB1, SB2, EGGBERT_DEMO, EGGBERT_VALUE, BLUPI_DEMO, INCOMPLETE, NONE }

    private GameFolder() {}

    static Kind recognize(File dir) {
        File data = child(dir, "DATA", true);
        File image = child(dir, "IMAGE08", true);
        File demoImage = image == null ? child(dir, "IMAGE", true) : null;
        if (data == null && image == null && demoImage == null) return Kind.NONE;
        if (data == null || (image == null && demoImage == null)) return Kind.INCOMPLETE;
        if (child(data, "CONFIG.DEF", false) == null || child(image != null ? image : demoImage, "INIT.BLP", false) == null) {
            return Kind.INCOMPLETE;
        }
        if (child(data, "DEMO200.BLP", false) != null) {
            if (demoImage != null) return Kind.BLUPI_DEMO;
            if (child(dir, "WCSUP.DLL", false) != null) return Kind.EGGBERT_DEMO;
            if (startsWith(dir, "SPEEDYEGGBERTSEVALUEWARE", ".EXE")) return Kind.EGGBERT_VALUE;
            return Kind.SB1;
        }
        if (demoImage == null && child(data, "DEMO300.BLP", false) != null) return Kind.SB2;
        return Kind.INCOMPLETE;
    }

    // A game folder of the given kind: the chosen folder or one level below.
    static File find(File dir, Kind kind) {
        if (recognize(dir) == kind) return dir;
        File[] children = dir.listFiles();
        if (children == null) return null;
        for (File c : children) {
            if (c.isDirectory() && recognize(c) == kind) return c;
        }
        return null;
    }

    // The most specific reason a folder (or one below it) holds no usable game.
    static Kind explain(File dir) {
        Kind best = recognize(dir);
        File[] children = dir.listFiles();
        if (children != null) {
            for (File c : children) {
                if (!c.isDirectory()) continue;
                Kind k = recognize(c);
                if (k.ordinal() < best.ordinal()) best = k;
            }
        }
        return best;
    }

    private static boolean startsWith(File dir, String prefix, String suffix) {
        File[] children = dir.listFiles();
        if (children == null) return false;
        for (File c : children) {
            String n = c.getName().toUpperCase(java.util.Locale.ROOT);
            if (c.isFile() && n.startsWith(prefix) && n.endsWith(suffix)) return true;
        }
        return false;
    }

    private static File child(File dir, String name, boolean directory) {
        File[] children = dir.listFiles();
        if (children == null) return null;
        for (File c : children) {
            if (c.getName().equalsIgnoreCase(name) && c.isDirectory() == directory) return c;
        }
        return null;
    }
}
