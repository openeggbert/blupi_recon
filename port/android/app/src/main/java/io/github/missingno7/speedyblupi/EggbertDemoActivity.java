package io.github.missingno7.speedyblupi;

// The Speedy Eggbert Demo (eGames, 1999): libeggbert_demo.so (port/games/sb1/editions.cmake).
public class EggbertDemoActivity extends EditionActivity {
    @Override
    GameFolder.Kind kind() { return GameFolder.Kind.EGGBERT_DEMO; }

    @Override
    String library() { return "eggbert_demo"; }
}
