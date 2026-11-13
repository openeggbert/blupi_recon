package io.github.missingno7.speedyblupi;

// The June 1998 Speedy Blupi Demo (Epsitec): libblupi_demo.so (port/games/sb1/editions.cmake).
public class BlupiDemoActivity extends EditionActivity {
    @Override
    GameFolder.Kind kind() { return GameFolder.Kind.BLUPI_DEMO; }

    @Override
    String library() { return "blupi_demo"; }
}
