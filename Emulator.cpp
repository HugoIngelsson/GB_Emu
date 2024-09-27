#include "Emulator.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <thread>

Emulator::Emulator(bool color_on) {
    this->color_on = color_on;
    cartridge = new ROM(color_on);
    cpu = new CPU();
    cpu->color_on = color_on;
}

Emulator::~Emulator() {
    delete cpu;
    cpu = NULL;
    delete cartridge;
    cartridge = NULL;
}

void Emulator::init() {
    isRunning = true;
}

void Emulator::setDebug() {
    debug = true;
}

int Emulator::load(string file) {
    return cartridge->load(file);
}

int my_row = 0;
int Emulator::run(string file) {
    // initialize
    init();

    begin:
    if (file.empty()) {
        cout << "Error: no file path specified!" << endl;
        return 1;
    }

    if (load(file)) {
        cout << "Error: no ROM loaded!" << endl;
        return 1;
    }

    if (cpu->init(cartridge)) {
        cout << "Error: CPU couldn't be initialized!" << endl;
        return 1;
    }

    cartridge->printROMinfo();
    PPU *ppu = new PPU();
    ppu->color_on = this->color_on;
    ppu->init(cpu);
    SDL_RenderClear(ppu->renderer);

    Logger *logger = new Logger();
    logger->open("log.txt");
    logger->writeLog(cpu);

    // cpu->startupCircumvention();

    SDL_Event e;
    bool quit = false;
    while (!quit) {
        // 4 T-cycles in an M-cycle
        t_cycle_backlog += cpu->executeOP() * cpu->speed;
        // logger->writeLog(cpu);
        cpu->eiPostExecute();

        if (cpu->in_DMA_transfer)
            cpu->executeDMA(t_cycle_backlog / cpu->speed);
        cpu->executeTimers(t_cycle_backlog / cpu->speed);
        ppu->dot(t_cycle_backlog);

        if (cpu->in_HDMA_transfer)
            t_cycle_backlog = 0;
        else
            t_cycle_backlog = cpu->interruptHander() * cpu->speed;

        if (ppu->frame_ready) {
            // printf("%d\n", test++);
            ppu->renderFrame();

            if (!cpu->key_map[9])
                this_thread::sleep_for(std::chrono::microseconds((int)(1000000 / FRAMES_PER_SEC)));

            cpu->readJOYP();
            quit = cpu->isQuit();
        }
    }

    delete ppu;
    ppu = NULL;
    delete logger;
    logger = NULL;

    return 0;
}