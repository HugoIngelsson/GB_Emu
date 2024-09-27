#include "PPU.h"
#include "Emulator.h"
#include <thread>
#include <stdio.h>

int main(int argc, char** argv) {
    bool color_on = false;
    for (int i=1; i<argc-1; i++) {
        if (argv[i][1] == 'c') color_on = true;
    }

    Emulator *emu = new Emulator(color_on);
    emu->run(argv[argc-1]);

    delete emu;
}