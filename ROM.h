#ifndef ROM_H
#define ROM_H

#include <string.h>
#include <fstream>
#include <iostream>
#include <map>

using namespace std;

class ROM {
public:
    ROM(bool color_on);
    ~ROM();

    void init();
    int load(string file);
    void saveFile();
    string file_name;
    string file_path;
    string file_raw_name;

    void setDebug();
    void printROMinfo();
    
    uint8_t *getROMbank(int bank_id);
    uint8_t *getRAMbank(int bank_id);

    uint8_t readBoot(uint16_t mem_address);

    bool isCGBGame();

    bool MBC1 = false;
    bool MBC3 = false;
    bool MBC5 = false;

    uint32_t RAM_size = 0;
    uint32_t ROM_size = 0;
    
private:
    bool debug = false;

    uint8_t *ROM_data = NULL;
    uint8_t *RAM_data = NULL;

    uint8_t *nintendo_logo = NULL;
    uint8_t *title = NULL;
    uint8_t *manu_code = NULL;
    uint8_t *CGB_flag = NULL;
    uint8_t *new_licensee_code = NULL;
    uint8_t *SGB_flag = NULL;
    uint8_t *cart_type = NULL;

    uint8_t *ROM_size_info = NULL;
    uint32_t num_ROM_banks = 0;
    uint8_t *RAM_size_info = NULL;
    uint32_t num_RAM_banks = 0;

    uint8_t *dest_code = NULL;
    uint8_t *old_licensee_code = NULL;
    uint8_t *mask_ROM_v = NULL;
    uint8_t *header_checksum = NULL;
    uint8_t *global_checksum = NULL;

    string const dmg_boot_name = "../boot_roms/dmg_boot.bin";
    string const cgb_boot_name = "../boot_roms/cgb_boot.bin";
    uint8_t *boot_rom = NULL;

    map<int, string> cartridge_types{
        {0x00, "ROM ONLY"},
        {0x01, "MBC1"},
        {0x02, "MBC1+RAM"},
        {0x03, "MBC1+RAM+BATTERY"},
        {0x05, "MBC2"},
        {0x06, "MBC2+BATTERY"},
        {0x08, "ROM+RAM"},
        {0x09, "ROM+RAM+BATTERY"},
        {0x0B, "MMM01"},
        {0x0C, "MMM01+RAM"},
        {0x0D, "MMM01+RAM+BATTERY"},
        {0x0F, "MBC3+TIMER+BATTERY"},
        {0x10, "MBC3+TIMER+RAM+BATTERY"},
        {0x11, "MBC3"},
        {0x12, "MBC3+RAM"},
        {0x13, "MBC3+RAM+BATTERY"},
        {0x19, "MBC5"},
        {0x1A, "MBC5+RAM"},
        {0x1B, "MBC5+RAM+BATTERY"},
        {0x1C, "MBC5+RUMBLE"},
        {0x1D, "MBC5+RUMBLE+RAM"},
        {0x1E, "MBC5+RUMBLE+RAM+BATTERY"},
        {0x20, "MBC6"},
        {0x22, "MBC7+SENSOR+RUMBLE+RAM+BATTERY"},
        {0xFC, "POCKET CAMERA"},
        {0xFD, "BANDAI TAMA5"},
        {0xFE, "HuC3"},
        {0xFF, "HuC1+RAM+BATTERY"}
    };
};

#endif