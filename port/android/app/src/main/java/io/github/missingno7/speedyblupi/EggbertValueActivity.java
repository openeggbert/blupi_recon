package io.github.missingno7.speedyblupi;

// Speedy Eggbert SE Valueware (eGames, 1999): libeggbert_value.so (port/games/sb1/editions.cmake).
public class EggbertValueActivity extends EditionActivity {
    @Override
    GameFolder.Kind kind() { return GameFolder.Kind.EGGBERT_VALUE; }

    @Override
    String library() { return "eggbert_value"; }
}
