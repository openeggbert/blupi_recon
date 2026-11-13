package io.github.missingno7.speedyblupi;

import java.io.File;

// An edition with a program of its own (port/docs/EDITIONS.md): SDL's activity running that edition's
// library, in a process of its own (AndroidManifest.xml) so that each game has its own SDL state, with
// its import (SetupActivity.folderName) as the game data and a user folder of its own, so that a demo's
// saves never mix with the full game's.
abstract class EditionActivity extends BlupiActivity {
    abstract GameFolder.Kind kind();
    abstract String library();

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", library() };
    }

    @Override
    protected String[] getArguments() {
        String folder = SetupActivity.folderName(kind());
        File data = new File(getFilesDir(), folder);
        File external = getExternalFilesDir(null);
        if (GameFolder.recognize(data) != kind() && external != null &&
            GameFolder.recognize(new File(external, folder)) == kind()) {
            data = new File(external, folder);   // development: data pushed with adb
        }
        File user = new File(getFilesDir(), library());
        if (!user.isDirectory()) user.mkdirs();
        String[] development = super.getArguments();
        String[] args = new String[development.length + 4];
        args[0] = "--data";
        args[1] = data.getPath();
        args[2] = "--user";
        args[3] = user.getPath();
        System.arraycopy(development, 0, args, 4, development.length);
        return args;
    }
}
