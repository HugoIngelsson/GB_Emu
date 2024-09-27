#ifndef CPU_H
#define CPU_H
#include <vector>
#include <array>
#include <ctime>
#include "ROM.h"
#include "SDL.h"

using namespace std;

class CPU {
public:
    CPU();
    ~CPU();
    int init(ROM *cartridge);
    ROM *cartridge;
    int executeOP();
    void close();

    uint16_t read(uint16_t mem_address);
    uint16_t readVRAM(uint16_t mem_address, int bank);
    int write(uint16_t mem_address, uint8_t value);
    void overrideSTAT(uint8_t value);

    bool debug = false;

    bool booting = true;
    void startupCircumvention();

    void eiPostExecute();
    int interruptHander();
    void executeTimers(int32_t new_cycles);

    void readJOYP();
    bool isQuit();
    bool key_map[10] = {};

    bool in_DMA_transfer = false;
    void executeDMA(uint32_t new_cycles);
    bool in_HDMA_transfer = false;
    bool hblank_DMA = false;
    bool ready_for_hblank_DMA = true;
    int data_transferred = 0x00;

    uint16_t pc = 0x0000;
    uint16_t sp = 0x0000;
    uint16_t af = 0x0000;
    uint8_t readR8(int target);

    bool color_on = false;
    int VRAM_bank = 0;
    uint32_t true_BG_COLOR[32];
    uint32_t true_OBJ_COLOR[32];
    int speed = 4;

private:
    // Registers
    uint16_t bc = 0x0000;
    uint16_t de = 0x0000;
    uint16_t hl = 0x0000;

    // Memory Bus
    uint8_t *ROM_bank_0;
    uint8_t *ROM_bank_N;
    uint8_t *VRAM;
    uint8_t *EXT_RAM;
    uint8_t *WRAM_0;
    uint8_t *WRAM_N;
    uint8_t *ECHO_RAM;
    uint8_t *OAM;
    uint8_t *IO_registers;
    uint8_t *HRAM;
    uint8_t *IE;

    uint8_t *VRAM_0, *VRAM_1;
    uint8_t OBJ_COLOR[64];
    uint8_t BG_COLOR[64];
    uint8_t clock_registers[16] = {};
    uint8_t latck_clock_register = 0xff;
    void latchClock();

    // MBC write-only variables
    uint8_t RAM_enable;
    uint16_t RAM_bank_number;
    uint16_t ROM_bank_number;
    bool ROM_RAM_mode_select;

    // Interrupt master enable flag [write only]
    bool IME = false;

    uint8_t ei_timer = 0;
    bool halt = false;
    bool halt_bug = false;
    uint16_t const interruptHandlers[5] = {0x0040, 0x0048, 0x0050, 0x0058, 0x0060};

    bool getInterruptMaster();
    uint8_t getInterruptFlag();
    uint8_t getInterruptEnable();
    int enterInterrupt(int bit);

    int32_t m_cycles_for_div = 0;
    int32_t m_cycles_for_tima = 0;
    int32_t const tac_clock_select[4] = {256, 4, 16, 64};

    uint16_t DMA_source_base = 0x0000;
    uint16_t DMA_add = 0x00;

    int  getZeroFlag();
    void setZeroFlag(int i);
    int  getSubtractionFlag();
    void setSubtractionFlag(int i);
    int  getHalfCarryFlag();
    void setHalfCarryFlag(int i);
    int  getCarryFlag();
    void setCarryFlag(int i);

    bool causesHalfAddOverflow(uint8_t a, uint8_t b);
    bool causesAddOverflow(uint8_t a, uint8_t b); 
    bool causesAddOverflow(uint16_t a, uint16_t b); 

    void writeR8(int target, uint16_t value);
    uint16_t readR16(int target);
    uint16_t readR16mem(int target);
    void writeR16(int target, uint16_t value);
    bool getCallCondition(int target);
};

#endif
