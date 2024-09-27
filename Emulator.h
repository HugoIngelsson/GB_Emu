#ifndef EMULATOR_H
#define EMULATOR_H

#include "ROM.h"
#include "CPU.h"
#include "PPU.h"
#include "SDL.h"
#include "Logger.h"
#include <string.h>
#include <fstream>
#include <iostream>

using namespace std;

class Emulator {
public:
    Emulator(bool color_on);
    ~Emulator();
    int load(string file);
    int run(string file);
    void setDebug();
private:
    ROM* cartridge;
    CPU* cpu;

    void init();
    bool isRunning = false;
    bool isLoaded;
    bool debug = false;
    bool color_on = false;
    int t_cycle_backlog = 0;

    int test = 0;
};

#endif