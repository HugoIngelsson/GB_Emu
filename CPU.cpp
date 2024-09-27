#include "CPU.h"

CPU::CPU() {
}

CPU::~CPU() {
    close();
}

int CPU::init(ROM *cartridge) {
    this->cartridge = cartridge;
    ROM_bank_0 =    cartridge->getROMbank(0);
    ROM_bank_N =    cartridge->getROMbank(1);
    VRAM_0 =        (uint8_t*)calloc(0x2000, sizeof(uint8_t));
    VRAM_1 =        (uint8_t*)calloc(0x2000, sizeof(uint8_t));
    VRAM =          VRAM_0;
    EXT_RAM =       cartridge->getRAMbank(0);
    WRAM_0 =        (uint8_t*)calloc(0x8000, sizeof(uint8_t));
    WRAM_N =        WRAM_0 + 0x1000;
    ECHO_RAM =      WRAM_0;
    OAM =           (uint8_t*)calloc(0xA0, sizeof(uint8_t));
    IO_registers =  (uint8_t*)calloc(0x80, sizeof(uint8_t));
    HRAM =          (uint8_t*)calloc(0x7F, sizeof(uint8_t));
    IE =            (uint8_t*)calloc(0x1, sizeof(uint8_t));

    if (VRAM_0 == NULL || WRAM_0 == NULL || OAM == NULL || 
        IO_registers == NULL || HRAM == NULL || IE == NULL) {
            
        cout << "Out of memory! Not enough space for CPU." << endl;
        return 1;
    }

    ifstream saved_ram(cartridge->file_path + "/saves/" + cartridge->file_raw_name + ".wram", ios::in|ios::binary|ios::ate);
    if (saved_ram.good()) {
        int fsize = saved_ram.tellg();
        saved_ram.seekg(ios::beg);

        if (fsize <= 0x8000) {
            saved_ram.read((char*)WRAM_0, fsize);
            printf("WRAM loaded\n");
        }
    }

    return 0;
}

void CPU::close() {
    std::ofstream out(cartridge->file_path + "/saves/" + cartridge->file_raw_name + ".wram", std::ofstream::binary);

    if (EXT_RAM != NULL) {
        out.write((char*)EXT_RAM, 0x2000);
        printf("WRAM saved\n");
    }

    out.close();

    free(VRAM_0);
    free(VRAM_1);
    free(WRAM_0);
    free(OAM);
    free(IO_registers);
    free(HRAM);
    free(IE);
}

uint16_t CPU::read(uint16_t mem_address) {
    if (mem_address < 0x4000) {
        if (booting) {
            if (mem_address < 0x0100 || (mem_address >= 0x0200 && mem_address < 0x08FF && color_on)) {
                return cartridge->readBoot(mem_address);
            }
        }
        return *(ROM_bank_0     + mem_address - 0x0000);
    } else if (mem_address < 0x8000) {
        return *(ROM_bank_N     + mem_address - 0x4000);
    } else if (mem_address < 0xA000) {
        return *(VRAM           + mem_address - 0x8000);
    } else if (mem_address < 0xC000) {
        if (cartridge->MBC3 && RAM_bank_number > 0x03) {
            return clock_registers[RAM_bank_number];
        }
        else if ((RAM_enable & 0xF) == 0xA && EXT_RAM != NULL) {
            return *(EXT_RAM    + mem_address - 0xA000);
        }
        else    // RAM is disconnected
            return 0xFF;
    } else if (mem_address < 0xD000) {
        return *(WRAM_0         + mem_address - 0xC000);
    } else if (mem_address < 0xE000) {
        return *(WRAM_N         + mem_address - 0xD000);
    } else if (mem_address < 0xFE00) {
        return *(ECHO_RAM       + mem_address - 0xE000);
    } else if (mem_address < 0xFEA0) {
        return *(OAM            + mem_address - 0xFE00);
    } else if (mem_address < 0xFF00) {
        // prohibited memory address, shouldn't
        // be accessed
        return 0xFF;
    } else if (mem_address < 0xFF80) {
        return *(IO_registers   + mem_address - 0xFF00);
    } else if (mem_address < 0xFFFF) {
        return *(HRAM           + mem_address - 0xFF80);
    } else {
        return *IE;
    }
}

uint16_t CPU::readVRAM(uint16_t mem_address, int bank) {
    if (mem_address >= 0x8000 && mem_address < 0xA000) {
        if (bank == 0)
            return *(VRAM_0 + mem_address - 0x8000);
        if (bank == 1)
            return *(VRAM_1 + mem_address - 0x8000);
    }

    return 0x0000;
}

int CPU::write(uint16_t mem_address, uint8_t value) {
    if (mem_address < 0x8000) {
        if (cartridge->MBC1) {
            if (mem_address < 0x2000) {
                RAM_enable = value;
            } else if (mem_address < 0x4000) {
                if (value == 0) value++;
                ROM_bank_number &= 0x60;
                ROM_bank_number |= (value & 0x1f);
                ROM_bank_N = cartridge->getROMbank(ROM_bank_number);
            } else if (mem_address < 0x6000) {
                if (cartridge->RAM_size == 0x8000) {
                    RAM_bank_number = value;
                    EXT_RAM = cartridge->getRAMbank(RAM_bank_number);
                }
                else if (cartridge->ROM_size >= 0x100000) {
                    ROM_bank_number &= 0x1f;
                    ROM_bank_number |= ((value & 0x03) << 5);
                }
            } else {
                ROM_RAM_mode_select = (value != 0);
            }
        } else if (cartridge->MBC3) {
            if (mem_address < 0x2000) {
                RAM_enable = value;
            } else if (mem_address < 0x4000) {
                if (value == 0) value++;
                ROM_bank_number = (value & 0x7f);
                ROM_bank_N = cartridge->getROMbank(ROM_bank_number);
            } else if (mem_address < 0x6000) {
                if (value <= 0x03) {
                    RAM_bank_number = value;
                    EXT_RAM = cartridge->getRAMbank(RAM_bank_number);
                } else {
                    RAM_bank_number = value;
                }
            } else {
                if (latck_clock_register == 0x00 && value == 0x01) {
                    latchClock();
                }
                latck_clock_register = value;
            }
        } else if (cartridge->MBC5) {
            if (mem_address < 0x2000) {
                RAM_enable = value;
            } else if (mem_address < 0x3000) {
                ROM_bank_number &= 0xff00;
                ROM_bank_number |= value;
                ROM_bank_N = cartridge->getROMbank(ROM_bank_number);
            } else if (mem_address < 0x4000) {
                ROM_bank_number &= 0x00ff;
                ROM_bank_number |= (value & 0x01) << 8;
                ROM_bank_N = cartridge->getROMbank(ROM_bank_number);
            } else if (mem_address < 0x6000) {
                RAM_bank_number = value;
                EXT_RAM = cartridge->getRAMbank(RAM_bank_number);
            }
        }
    } else if (mem_address < 0xA000) {
        *(VRAM + mem_address - 0x8000) = value;
    } else if (mem_address < 0xC000) {
        if (cartridge->MBC3 && RAM_bank_number > 0x03)
            clock_registers[RAM_bank_number] = value;
        if ((RAM_enable & 0xF) == 0xA && EXT_RAM != NULL)
            *(EXT_RAM + mem_address - 0xA000) = value;
        else
            return 1;
    } else if (mem_address < 0xD000) {
        *(WRAM_0 + mem_address - 0xC000) = value;
    } else if (mem_address < 0xE000) {
        *(WRAM_N + mem_address - 0xD000) = value;
    } else if (mem_address < 0xFE00) {
        *(ECHO_RAM + mem_address - 0xE000) = value;
    } else if (mem_address < 0xFEA0) {
        *(OAM + mem_address - 0xFE00) = value;
    } else if (mem_address < 0xFF00) {
        // prohibited memory address, shouldn't
        // be accessed
        return 1;
    } else if (mem_address < 0xFF80) {
        if (mem_address == 0xFF00) {
            *IO_registers = value & 0x30;
            readJOYP();
        }
        else if (mem_address == 0xFF04) { // writing to DIV resets it
            // but no it doesn't? at least it doesn't seem that way, 
            // this led to bug in Pokemon Red (no wild encounters)
            *(IO_registers + 0x0004) = value;
        }
        else if (mem_address == 0xFF41) { // STAT is not fully writeable
            *(IO_registers + mem_address - 0xFF00) &= 0x07;
            *(IO_registers + mem_address - 0xFF00) |= value & 0xf8;
        }
        else if (mem_address == 0xFF46) { // start DMA transfer
            in_DMA_transfer = true;
            DMA_source_base = (uint16_t)value << 8;
            DMA_add = 0x00;
            // printf("ENTERED DMA TRANSFER\n");
        }
        else if (mem_address == 0xFF4D && color_on) {
            // printf("MESSING WITH SPEED SWITCH %02X\n", value);
            *(IO_registers + mem_address - 0xFF00) = value;
        }
        else if (mem_address == 0xFF4F && color_on) {
            // printf("SWITCHED VRAM BANKS TO %d\n", 0x01 & value);

            if (value & 0x01) {
                VRAM = VRAM_1;
                VRAM_bank = 1;
            } else {
                VRAM = VRAM_0;
                VRAM_bank = 0;
            }

            *(IO_registers + mem_address - 0xFF00) = (value & 0x01) | 0xfe;
        }
        else if (mem_address == 0xFF50) {
            *(IO_registers + mem_address - 0xFF00) = value;
            booting = false;
        }
        else if (mem_address == 0xFF55 && color_on) {
            // printf("ENTERED HDMA TRANSFER\n");
            if (in_HDMA_transfer) {
                if (!(value & 0x80)) {
                    in_HDMA_transfer = false;
                    *(IO_registers + mem_address - 0xFF00) |= 0x80;
                }
            } else {
                hblank_DMA = (value & 0x80);
                *(IO_registers + mem_address - 0xFF00) = value & 0x7f;
                data_transferred = 0;
                ready_for_hblank_DMA = false;
                in_HDMA_transfer = true;
            }
        }
        else if (mem_address == 0xFF69 && color_on) {
            int address = read(0xff68) & 0x3f;
            if (read(0xff68) & 0x80)
                write(0xff68, (read(0xff68)+1) & 0xbf);

            BG_COLOR[address] = value;
            int color_address = address/2;
            int R = BG_COLOR[color_address*2] & 0x1f;
            int G = ((BG_COLOR[color_address*2] & 0xe0) >> 5) | ((BG_COLOR[color_address*2+1] & 0x03) << 3);
            int B = (BG_COLOR[color_address*2+1] & 0x7c) >> 2;
            true_BG_COLOR[color_address] = 0xff000000 | (R << 19) | (G << 11) | (B << 3);
        }
        else if (mem_address == 0xFF6B && color_on) {
            int address = read(0xff6A) & 0x3f;
            if (read(0xff6A) & 0x80)
                write(0xff6A, (read(0xff6A)+1) & 0xbf);

            OBJ_COLOR[address] = value;
            int color_address = address/2;
            int R = OBJ_COLOR[color_address*2] & 0x1f;
            int G = ((OBJ_COLOR[color_address*2] & 0xe0) >> 5) | ((OBJ_COLOR[color_address*2+1] & 0x03) << 3);
            int B = (OBJ_COLOR[color_address*2+1] & 0x7c) >> 2;
            true_OBJ_COLOR[color_address] = 0xff000000 | (R << 19) | (G << 11) | (B << 3);
        }
        else if (mem_address == 0xFF70 && color_on) {
            int bank = value & 0x07;
            if (bank == 0) bank++;
            WRAM_N = WRAM_0 + 0x1000 * bank;
            // printf("SET WRAM TO BANK %d\n", bank);

            *(IO_registers + mem_address - 0xFF00) = value;
        }
        else {
            *(IO_registers + mem_address - 0xFF00) = value;
        }
    } else if (mem_address < 0xFFFF) {
        *(HRAM + mem_address - 0xFF80) = value;
    } else {
        *IE = value;
    }

    return 0;
}

void CPU::overrideSTAT(uint8_t value) {
    *(IO_registers + 0x41) = value;
}

int CPU::getZeroFlag() {
    return (af&0x0080) > 0 ? 1 : 0;
}

void CPU::setZeroFlag(int i) {
    af &= 0xff70;
    if (i)
        af |= 0x80;
}

int CPU::getSubtractionFlag() {
    return (af&0x0040) > 0 ? 1 : 0;
}

void CPU::setSubtractionFlag(int i) {
    af &= 0xffb0;
    if (i)
        af |= 0x40;
}

int CPU::getHalfCarryFlag() {
    return (af&0x0020) > 0 ? 1 : 0;
}

void CPU::setHalfCarryFlag(int i) {
    af &= 0xffd0;
    if (i)
        af |= 0x20;
}

bool CPU::causesHalfAddOverflow(uint8_t a, uint8_t b) {
    uint8_t lowA = a & 0x0f, lowB = b & 0x0f;
    return lowA + lowB > 0x0f;
}

int CPU::getCarryFlag() {
    return (af&0x0010) > 0 ? 1 : 0;
}

void CPU::setCarryFlag(int i) {
    af &= 0xffe0;
    if (i)
        af |= 0x10;
}

bool CPU::causesAddOverflow(uint8_t a, uint8_t b) {
    return (uint16_t)a + (uint16_t)b > 0xff;
}

bool CPU::causesAddOverflow(uint16_t a, uint16_t b) {
    return a + b < a;
}

uint8_t CPU::readR8(int target) {
    switch (target) {
        case 0: // b
            return (bc & 0xff00) >> 8;
        case 1: // c
            return (bc & 0x00ff) >> 0;
        case 2: // d
            return (de & 0xff00) >> 8;
        case 3: // e
            return (de & 0x00ff) >> 0;
        case 4: // h
            return (hl & 0xff00) >> 8;
        case 5: // l
            return (hl & 0x00ff) >> 0;
        case 6: // [hl]
            return read(hl);
        case 7: // a
            return (af & 0xff00) >> 8;
    }

    // something went wrong
    printf("readR8 invalid argument: %d", target);
    return 0xff;
}

void CPU::writeR8(int target, uint16_t value) {
    switch (target) {
        case 0: // b
            bc = (bc & 0x00ff) | (value << 8);
            break;
        case 1: // c
            bc = (bc & 0xff00) | (value << 0);
            break;
        case 2: // d
            de = (de & 0x00ff) | (value << 8);
            break;
        case 3: // e
            de = (de & 0xff00) | (value << 0);
            break;
        case 4: // h
            hl = (hl & 0x00ff) | (value << 8);
            break;
        case 5: // l
            hl = (hl & 0xff00) | (value << 0);
            break;
        case 6: // [hl]
            write(hl, (uint8_t)value);
            break;
        case 7: // a
            af = (af & 0x00ff) | (value << 8);
            break;
        default:
            printf("writeR8 invalid argument: %d", target);
    }
}

void printR8(int target) {
    switch (target) {
        case 0: // b
            printf("b");
            break;
        case 1: // c
            printf("c");
            break;
        case 2: // d
            printf("d");
            break;
        case 3: // e
            printf("e");
            break;
        case 4: // h
            printf("h");
            break;
        case 5: // l
            printf("l");
            break;
        case 6: // [hl]
            printf("[hl]");
            break;
        case 7: // a
            printf("a");
            break;
    }
}

uint16_t CPU::readR16(int target) {
    switch (target) {
        case 0: // bc
            return bc;
        case 1: // de
            return de;
        case 2: // hl
            return hl;
        case 3: // sp
            return sp;
    }

    // something went wrong
    printf("readR16 invalid argument: %d", target);
    return 0xffff;
}

uint16_t CPU::readR16mem(int target) {
    switch (target) {
        case 0: // bc
            return bc;
        case 1: // de
            return de;
        case 2: // hl+
            return hl++;
        case 3: // hl-
            return hl--;
    }

    // something went wrong
    printf("readR16mem invalid argument: %d", target);
    return 0xffff;
}

void CPU::writeR16(int target, uint16_t value) {
    switch (target) {
        case 0: // bc
            bc = value;
            break;
        case 1: // de
            de = value;
            break;
        case 2: // hl
            hl = value;
            break;
        case 3: // sp
            sp = value;
            break;
        default:
            printf("writeR16 invalid argument: %d", target);
    }
}

void printR16(int target) {
    switch (target) {
        case 0: // bc
            printf("bc");
            break;
        case 1: // de
            printf("de");
            break;
        case 2: // hl
            printf("hl");
            break;
        case 3: // sp
            printf("sp");
            break;
    }
}

void printR16mem(int target) {
    switch (target) {
        case 0: // bc
            printf("bc");
            break;
        case 1: // de
            printf("de");
            break;
        case 2: // hl
            printf("hl+");
            break;
        case 3: // sp
            printf("hl-");
            break;
    }
}

void printR16stk(int target) {
    switch (target) {
        case 0: // bc
            printf("bc");
            break;
        case 1: // de
            printf("de");
            break;
        case 2: // hl
            printf("hl");
            break;
        case 3: // sp
            printf("af");
            break;
    }
}

bool CPU::getCallCondition(int target) {
    switch (target) {
        case 0: // nz
            return !getZeroFlag();
        case 1: // z
            return getZeroFlag();
        case 2: // nc
            return !getCarryFlag();
        case 3: // c
            return getCarryFlag();
    }

    // something went wrong
    printf("getCallCondition invalid argument: %d", target);
    return false;
}

void printCond(int target) {
    switch (target) {
        case 0: // nz
            printf("nz");
            break;
        case 1: // z
            printf("z");
            break;
        case 2: // nc
            printf("nc");
            break;
        case 3: // c
            printf("c");
            break;
    }
}

uint8_t CPU::getInterruptEnable() {
    return *IE;
}

uint8_t CPU::getInterruptFlag() {
    return *(IO_registers + 0x000F);
}

bool CPU::getInterruptMaster() {
    return IME;
}

int CPU::enterInterrupt(int bit) {
    IME = false;
    write(0xff0f, read(0xff0f) & (~(1 << bit)));
    
    write(--sp, (pc & 0xff00) >> 8);
    write(--sp, pc & 0x00ff);
    pc = interruptHandlers[bit];

    return 5;
}

void CPU::eiPostExecute() {
    if (ei_timer != 0) {
        ei_timer--;
        if (ei_timer == 0) {
            IME = true;
        }
    }
}

int CPU::interruptHander() {
    if (getInterruptEnable() & getInterruptFlag()) {
        if (halt) {
            halt = false;
        }
        if (IME) {
            uint8_t interrupts = (getInterruptEnable() & getInterruptFlag());
            for (int bit=0; bit<5; bit++) {
                if (interrupts & (0x1 << bit)) {
                    // printf("%d, %02X,  %02X\n", bit, getInterruptFlag(), *IO_registers);
                    return enterInterrupt(bit);
                }
            }
        }
    }

    return 0;
}

void CPU::readJOYP() {
    // printf("Called readJOYP, JOYP = %02X --> ", *IO_registers);
    uint8_t JOYP = *IO_registers | 0x0f;
    bool select = !(JOYP & 0x20);
    bool d_pad  = !(JOYP & 0x10);

    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            key_map[8] = true;
        }
        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            switch (e.key.keysym.sym) {
                case SDLK_ESCAPE:
                case SDLK_RETURN: key_map[0] = e.type == SDL_KEYDOWN; break;
                case SDLK_SPACE: key_map[1] = e.type == SDL_KEYDOWN; break;
                case SDLK_l: key_map[2] = e.type == SDL_KEYDOWN; break;
                case SDLK_k: key_map[3] = e.type == SDL_KEYDOWN; break;
                case SDLK_s: key_map[4] = e.type == SDL_KEYDOWN; break;
                case SDLK_w: key_map[5] = e.type == SDL_KEYDOWN; break;
                case SDLK_a: key_map[6] = e.type == SDL_KEYDOWN; break;
                case SDLK_d: key_map[7] = e.type == SDL_KEYDOWN; break;
                case SDLK_LSHIFT: key_map[9] = e.type == SDL_KEYDOWN; break;
            }
        }
    }

    if (select) {
        if (key_map[0])
            JOYP &= ~(0x08);
        if (key_map[1])
            JOYP &= ~(0x04);
        if (key_map[2])
            JOYP &= ~(0x02);
        if (key_map[3])
            JOYP &= ~(0x01);
    }
    if (d_pad) {
        if (key_map[4])
            JOYP &= ~(0x08);
        if (key_map[5])
            JOYP &= ~(0x04);
        if (key_map[6])
            JOYP &= ~(0x02);
        if (key_map[7])
            JOYP &= ~(0x01);
    }

    if (*IO_registers & ~(JOYP))
        write(0xff0f, read(0xff0f) | 0x10);

    *IO_registers = JOYP;
    // printf("%02X\n", *IO_registers);
}

bool CPU::isQuit() {
    return key_map[8];
}

void CPU::latchClock() {
    time_t t = time(0);
    tm* now = localtime(&t);
    // doesn't account for leap years, but not really super important
    int days_since_1900 = (now->tm_yday + 365*(now->tm_year)) % 512;
    
    clock_registers[0x08] = now->tm_sec;
    if (clock_registers[0x08] > 59) clock_registers[0x08] -= 60;
    clock_registers[0x09] = now->tm_min;
    clock_registers[0x0A] = now->tm_hour;
    clock_registers[0x0B] = days_since_1900 & 0x00ff;
    clock_registers[0x0C] = (days_since_1900 & 0x0100) >> 8;
}

void CPU::executeTimers(int32_t new_cycles) {
    m_cycles_for_div -= new_cycles;
    while (m_cycles_for_div <= 0) {
        write(0xff04, read(0xff04) + 1);
        m_cycles_for_div += 64;
    }

    uint8_t TAC = read(0xff07);
    if (TAC & 0x04) {
        m_cycles_for_tima -= new_cycles;
        while (m_cycles_for_tima <= 0) {
            m_cycles_for_tima += tac_clock_select[TAC & 0x3];
            if (read(0xff05) == 0xff) {
                write(0xff05, read(0xff06));
                write(0xff0f, read(0xff0f) | 0x04);
            }
            else {
                write(0xff05, read(0xff05)+1);
            }
        }
    }
}

void CPU::executeDMA(uint32_t new_cycles) {
    while (new_cycles-->0) {
        write(0xfe00 + DMA_add, read(DMA_source_base + DMA_add));
        DMA_add++;
        if (DMA_add == 0xA0) {
            in_DMA_transfer = false;
            return;
        }
    }
}

// not implemented: stop
int CPU::executeOP() {
    if (halt) return 1;
    if (debug) printf("%04X %04X ", pc, sp);

    if (in_HDMA_transfer) { // in HDMA
        uint16_t source = read(0xff52) | (read(0xff51) << 8);
        uint16_t dest = read(0xff54) | (read(0xff53) << 8);
        source &= 0xfff0; 
        source += data_transferred;
        dest &= 0x1ff0;
        dest += 0x8000 + data_transferred;

        if (hblank_DMA) {
            if (read(0xff41) & 0x03) { // not in hblank
                ready_for_hblank_DMA = true;
            } else if (ready_for_hblank_DMA) {
                for (uint8_t i = 0x00; i < 0x10; i++) {
                    write(dest+i, read(source+i));
                }
                data_transferred += 0x10;
                ready_for_hblank_DMA = false;

                if (read(0xff55) == 0) {
                    *(IO_registers + 0x0055) = 0xff;
                    in_HDMA_transfer = false;
                }
                else {
                    *(IO_registers + 0x0055) -= 1;
                }

                return 32 / speed;
            }
        } else {
            for (uint8_t i = 0x00; i < 0x10; i++) {
                write(dest+i, read(source+i));
            }
            data_transferred += 0x10;

            if (read(0xff55) == 0) {
                *(IO_registers + 0x0055) = 0xff;
                in_HDMA_transfer = false;
            }
            else {
                *(IO_registers + 0x0055) -= 1;
            }

            return 32 / speed;
        }
    }

    uint8_t imm8;
    uint16_t imm16;
    int r8, r8_2, r16, u3, cc, tgt3;
    uint8_t carry;
    int8_t imm8_signed;

    uint8_t op = read(pc++);
    // if (debug) printf("%02X ", op);

    if (halt_bug) {
        pc--;
        halt_bug = false;
    }

    switch ((op & 0xc0) >> 6) { // consider bits 6 & 7

    case 0x00: // block 0
        if (op & 0x04) {
            if ((op & 0x07) == 7) {
                switch (op) {
                    case 0x07:  // rlca
                        setCarryFlag((readR8(7) & 0x80) > 0);
                        writeR8(7, ((readR8(7) & 0x7f) << 1) | ((readR8(7) & 0x80) >> 7));
                        setZeroFlag(0);
                        setSubtractionFlag(0);
                        setHalfCarryFlag(0);

                        if (debug) {printf("rlca"); printf("\n");}
                        return 1;
                    case 0x0f:  // rrca
                        setCarryFlag((readR8(7) & 0x01) > 0);
                        writeR8(7, ((readR8(7) & 0xfe) >> 1) | ((readR8(7) & 0x01) << 7));
                        setZeroFlag(0);
                        setSubtractionFlag(0);
                        setHalfCarryFlag(0);

                        if (debug) {printf("rrca"); printf("\n");}
                        return 1;
                    case 0x17:  // rla
                        carry = getCarryFlag();
                        setCarryFlag((readR8(7) & 0x80) > 0);
                        writeR8(7, ((readR8(7) & 0x7f) << 1) | carry);
                        setZeroFlag(0);
                        setSubtractionFlag(0);
                        setHalfCarryFlag(0);

                        if (debug) {printf("rla"); printf("\n");}
                        return 1;
                    case 0x1f:  // rra
                        carry = getCarryFlag();
                        setCarryFlag((readR8(7) & 0x01) > 0);
                        writeR8(7, ((readR8(7) & 0xfe) >> 1) | (carry << 7));
                        setZeroFlag(0);
                        setSubtractionFlag(0);
                        setHalfCarryFlag(0);

                        if (debug) {printf("rra"); printf("\n");}
                        return 1;
                    case 0x27:  // daa
                        if (getSubtractionFlag()) { // last operation was subtraction
                            if (getCarryFlag()) writeR8(7, readR8(7) - 0x60);
                            if (getHalfCarryFlag()) writeR8(7, readR8(7) - 0x06);
                        }
                        else {  // last operation was addition
                            if (getCarryFlag() || readR8(7) > 0x99) {
                                writeR8(7, readR8(7) + 0x60);
                                setCarryFlag(1);
                            }
                            if (getHalfCarryFlag() || (readR8(7) & 0x0f) > 0x09) 
                                writeR8(7, readR8(7) + 0x06);
                        }
                        setZeroFlag(readR8(7) == 0);
                        setHalfCarryFlag(0);

                        if (debug) {printf("daa"); printf("\n");}
                        return 1;
                    case 0x2f:  // cpl
                        setSubtractionFlag(1);
                        setHalfCarryFlag(1);
                        writeR8(7, ~readR8(7));

                        if (debug) {printf("cpl"); printf("\n");}
                        return 1;
                    case 0x37:  // scf
                        setCarryFlag(1);
                        setHalfCarryFlag(0);
                        setSubtractionFlag(0);

                        if (debug) {printf("scf"); printf("\n");}
                        return 1;
                    case 0x3f:  // ccf
                        setHalfCarryFlag(0);
                        setSubtractionFlag(0);
                        setCarryFlag(!getCarryFlag());

                        if (debug) {printf("ccf"); printf("\n");}
                        return 1;
                }
            }
            else {
                switch (op & 0x03) {
                    case 0x00:  // inc r8
                        r8 = (op & 0x38) >> 3;
                        setHalfCarryFlag((readR8(r8) & 0x0f) == 0x0f);
                        writeR8(r8, (uint8_t)(readR8(r8) + 1));
                        setZeroFlag(readR8(r8) == 0);
                        setSubtractionFlag(0);

                        if (debug) {printf("inc "); printR8(r8); printf("\n");}
                        return 1 + (r8 == 6)*2;
                    case 0x01:  // dec r8
                        r8 = (op & 0x38) >> 3;
                        setHalfCarryFlag((readR8(r8) & 0x0f) == 0);
                        writeR8(r8, (uint8_t)(readR8(r8) - 1));
                        setZeroFlag(readR8(r8) == 0);
                        setSubtractionFlag(1);

                        if (debug) {printf("dec "); printR8(r8); printf("\n");}
                        return 1 + (r8 == 6)*2;
                    case 0x02:  // ld r8, imm8
                        imm8 = read(pc++);
                        r8 = (op & 0x38) >> 3;
                        writeR8(r8, imm8);

                        if (debug) {printf("ld "); printR8(r8); printf(", imm8"); printf("\n");}
                        return 2 + (r8 == 6);
                }
            }
        } else if ((op & 0x27) == 0x20) { // jr cond, imm8
            imm8_signed = read(pc++);
            cc = (op & 0x18) >> 3;

            if (debug) {printf("jr "); printCond(cc); printf(", imm8"); printf("\n");}
            if (getCallCondition(cc)) {
                pc += imm8_signed;
                return 3;
            }
            return 2;
        } else {
            switch (op & 0x0f) {
                case 0x00:
                    if (op == 0x00) {   // nop
                        if (debug) {printf("nop"); printf("\n");}
                        return 1;
                    } else {            // stop
                        // enter very low power mode
                        if (read(0xff4d) & 0x01) {
                            if (read(0xff4d) & 0x80) {
                                speed = 2;
                                write(0xff4d, read(0xff4d) & 0x7f);
                            }
                            else {
                                speed = 4;
                                write(0xff4d, read(0xff4d) | 0x80);
                            }

                            write(0xff4d, read(0xff4d) & 0xfe);
                        }
                        if (debug) {printf("stop"); printf("\n");}
                        return 0;
                    }
                case 0x01:  // ld r16, imm16
                    imm16 = read(pc++); imm16 |= (read(pc++) << 8);
                    r16 = (op & 0x30) >> 4;
                    writeR16(r16, imm16);

                    if (debug) {printf("ld "); printR16(r16); printf(", imm16"); printf("\n");}
                    return 3;
                case 0x02:  // ld [r16mem], a
                    r16 = (op & 0x30) >> 4;
                    write(readR16mem(r16), readR8(7));
                    
                    if (debug) {printf("ld ["); printR16mem(r16); printf("], a"); printf("\n");}
                    return 2;
                case 0x03:  // inc r16
                    r16 = (op & 0x30) >> 4;
                    writeR16(r16, readR16(r16) + 1);

                    if (debug) {printf("inc "); printR16(r16); printf("\n");}
                    return 2;
                case 0x08:
                    if (op == 0x18) {   // jr imm8
                        imm8_signed = read(pc++);
                        pc += imm8_signed;

                        if (debug) {printf("jr imm8"); printf("\n");}
                        return 3;
                    } else {    // ld [imm16], sp
                        imm16 = read(pc++); imm16 |= (read(pc++) << 8);
                        write(imm16, sp & 0xff);
                        write(imm16+1, (sp & 0xff00) >> 8);

                        if (debug) {printf("ld [imm16], sp"); printf("\n");}
                        return 5;
                    }
                case 0x09:  // add hl, r16
                    r16 = (op & 0x30) >> 4;
                    setCarryFlag((uint16_t)(hl + readR16(r16)) < hl);
                    setHalfCarryFlag((((hl & 0x0fff) + (readR16(r16) & 0x0fff)) & 0x1000) > 0);
                    setSubtractionFlag(0);
                    writeR16(2, hl + readR16(r16));

                    if (debug) {printf("add hl, "); printR16(r16); printf("\n");}
                    return 2;
                case 0x0a:  // ld a, [r16mem]
                    r16 = (op & 0x30) >> 4;
                    writeR8(7, read(readR16mem(r16)));

                    if (debug) {printf("ld a, ["); printR16mem(r16); printf("]\n");}
                    return 2;
                case 0x0b:  // dec r16
                    r16 = (op & 0x30) >> 4;
                    writeR16(r16, readR16(r16) - 1);

                    if (debug) {printf("dec "); printR16(r16); printf("\n");}
                    return 2;
            }
        }
        break;
    
    case 0x01: // block 1
        if (op == 0x76) {   // halt
            halt = true;
            if (!IME && (getInterruptEnable() & getInterruptFlag())) {
                halt_bug = true;
                halt = false;
            }

            if (debug) {printf("halt"); printf("\n");}
            return 1;
        } else {            // ld r8, r8
            r8 = op & 0x07;
            r8_2 = (op & 0x38) >> 3;
            writeR8(r8_2, readR8(r8));

            if (debug) {printf("ld "); printR8(r8_2); printf(", "); printR8(r8); printf("\n");}
            return 1 + (r8 == 6) + (r8_2 == 6);
        }
        break;
    
    case 0x02: // block 2
        r8 = op & 0x7;
        switch ((op & 0x38) >> 3) {
            case 0x00:  // add a, r8
                setCarryFlag(causesAddOverflow(readR8(7), readR8(r8)));
                setHalfCarryFlag(causesHalfAddOverflow(readR8(7), readR8(r8)));
                setSubtractionFlag(0);
                writeR8(7, readR8(7) + readR8(r8));
                setZeroFlag(readR8(7) == 0x00);

                if (debug) {printf("add a, "); printR8(r8); printf("\n");}
                return 1 + (r8 == 6);
            case 0x01:  // adc a, r8
                carry = getCarryFlag();
                setCarryFlag(causesAddOverflow(readR8(7), readR8(r8) + carry) |
                    causesAddOverflow(readR8(r8), carry));
                setHalfCarryFlag(causesHalfAddOverflow(readR8(7), readR8(r8) + carry) |
                    causesHalfAddOverflow(readR8(r8), carry));
                setSubtractionFlag(0);
                writeR8(7, readR8(7) + readR8(r8) + carry);
                setZeroFlag(readR8(7) == 0x00);

                if (debug) {printf("adc a, "); printR8(r8); printf("\n");}
                return 1 + (r8 == 6);
            case 0x02:  // sub a, r8
                setHalfCarryFlag((readR8(r8) & 0x0f) > (readR8(7) & 0x0f));
                setCarryFlag(readR8(r8) > readR8(7));
                setSubtractionFlag(1);
                writeR8(7, (uint8_t)(readR8(7) - readR8(r8)));
                setZeroFlag(readR8(7) == 0);

                if (debug) {printf("sub a, "); printR8(r8); printf("\n");}
                return 1 + (r8 == 6);
            case 0x03:  // sbc a, r8
                carry = getCarryFlag();
                setHalfCarryFlag((((readR8(7) & 0x0f) - (readR8(r8) & 0x0f) - 
                                    (carry & 0x0f)) & 0x10) > 0);
                setCarryFlag(readR8(r8) + carry > readR8(7));
                setSubtractionFlag(1);
                writeR8(7, (uint8_t)(readR8(7) - readR8(r8) - carry));
                setZeroFlag(readR8(7) == 0);

                if (debug) {printf("sbc a, "); printR8(r8); printf("\n");}
                return 1 + (r8 == 6);
            case 0x04:  // and a, r8
                setCarryFlag(0);
                setHalfCarryFlag(1);
                setSubtractionFlag(0);
                writeR8(7, readR8(7) & readR8(r8));
                setZeroFlag(readR8(7) == 0x00);

                if (debug) {printf("and a, "); printR8(r8); printf("\n");}
                return 1 + (r8 == 6);
            case 0x05:  // xor a, r8
                setCarryFlag(0);
                setHalfCarryFlag(0);
                setSubtractionFlag(0);
                writeR8(7, readR8(7) ^ readR8(r8));
                setZeroFlag(readR8(7) == 0x00);

                if (debug) {printf("xor a, "); printR8(r8); printf("\n");}
                return 1 + (r8 == 6);
            case 0x06:  //  or a, r8
                writeR8(7, readR8(7) | readR8(r8));
                setCarryFlag(0);
                setHalfCarryFlag(0);
                setSubtractionFlag(0);
                setZeroFlag(readR8(7) == 0);

                if (debug) {printf("or a, "); printR8(r8); printf("\n");}
                return 1 + (r8 == 6);
            case 0x07:  //  cp a, r8
                setZeroFlag(readR8(7) == readR8(r8));
                setSubtractionFlag(1);
                setHalfCarryFlag((readR8(7) & 0x0f) < (readR8(r8) & 0x0f));
                setCarryFlag(readR8(7) < readR8(r8));

                if (debug) {printf("cp a, "); printR8(r8); printf("\n");}
                return 1 + (r8 == 6);
        }
        break;
    
    case 0x03: // block 3
        switch (op) {
            case 0xc6:  // add a, imm8
                imm8 = read(pc++);
                setCarryFlag(causesAddOverflow(readR8(7), imm8));
                setHalfCarryFlag(causesHalfAddOverflow(readR8(7), imm8));
                setSubtractionFlag(0);
                writeR8(7, readR8(7) + imm8);
                setZeroFlag(readR8(7) == 0x00);

                if (debug) {printf("add a, imm8"); printf("\n");}
                return 2;
            case 0xce:  // adc a, imm8
                imm8 = read(pc++);
                carry = getCarryFlag();
                setCarryFlag(causesAddOverflow(readR8(7), imm8 + carry) |
                    causesAddOverflow(imm8, carry));
                setHalfCarryFlag(causesHalfAddOverflow(readR8(7), imm8 + carry) |
                    causesHalfAddOverflow(imm8, carry));
                setSubtractionFlag(0);
                writeR8(7, readR8(7) + imm8 + carry);
                setZeroFlag(readR8(7) == 0x00);

                if (debug) {printf("adc a, imm8"); printf("\n");}
                return 2;
            case 0xd6:  // sub a, imm8
                imm8 = read(pc++);
                setHalfCarryFlag((imm8 & 0x0f) > (readR8(7) & 0x0f));
                setCarryFlag(imm8 > readR8(7));
                setSubtractionFlag(1);
                writeR8(7, (uint8_t)(readR8(7) - imm8));
                setZeroFlag(readR8(7) == 0);

                if (debug) {printf("sub a, imm8"); printf("\n");}
                return 2;
            case 0xde:  // sbc a, imm8
                imm8 = read(pc++);
                carry = getCarryFlag();
                setHalfCarryFlag((((readR8(7) & 0xf) - (imm8 & 0xf) - (carry & 0xf)) & 0x10) > 0);
                setCarryFlag(imm8 + carry > readR8(7));
                setSubtractionFlag(1);
                writeR8(7, (uint8_t)(readR8(7) - imm8 - carry));
                setZeroFlag(readR8(7) == 0);

                if (debug) {printf("sbc a, imm8"); printf("\n");}
                return 2;
            case 0xe6:  // and a, imm8
                imm8 = read(pc++);
                setCarryFlag(0);
                setHalfCarryFlag(1);
                setSubtractionFlag(0);
                writeR8(7, readR8(7) & imm8);
                setZeroFlag(readR8(7) == 0x00);

                if (debug) {printf("and a, imm8"); printf("\n");}
                return 2;
            case 0xee:  // xor a, imm8
                imm8 = read(pc++);
                setCarryFlag(0);
                setHalfCarryFlag(0);
                setSubtractionFlag(0);
                writeR8(7, readR8(7) ^ imm8);
                setZeroFlag(readR8(7) == 0x00);

                if (debug) {printf("xor a, imm8"); printf("\n");}
                return 2;
            case 0xf6:  //  or a, imm8
                imm8 = read(pc++);
                writeR8(7, readR8(7) | imm8);
                setCarryFlag(0);
                setHalfCarryFlag(0);
                setSubtractionFlag(0);
                setZeroFlag(readR8(7) == 0);

                if (debug) {printf("or a, imm8"); printf("\n");}
                return 2;
            case 0xfe:  //  cp a, imm8
                imm8 = read(pc++);
                setZeroFlag(readR8(7) == imm8);
                setSubtractionFlag(1);
                setHalfCarryFlag((readR8(7) & 0x0f) < (imm8 & 0x0f));
                setCarryFlag(readR8(7) < imm8);

                if (debug) {printf("cp a = %02X, imm8 = %02X", readR8(7), imm8); printf("\n");}
                return 2;
            case 0xc9:  // ret
                r8 = read(sp++);
                r8_2 = read(sp++);
                pc = (r8 | (r8_2 << 8)) & 0xffff;
                
                if (debug) {printf("ret"); printf("\n");}
                return 4;
            case 0xd9:  // reti
                r8 = read(sp++);
                r8_2 = read(sp++);
                pc = r8 | (r8_2 << 8);

                // set IME
                IME = true;

                if (debug) {printf("reti"); printf("\n");}
                return 4;
            case 0xc3:  // jp imm16
                imm16 = read(pc++); imm16 |= (read(pc++) << 8);
                pc = imm16;

                if (debug) {printf("jp imm16"); printf("\n");}
                return 4;
            case 0xe9:  // jp hl
                pc = hl;

                if (debug) {printf("jp hl"); printf("\n");}
                return 1;
            case 0xcd:  // call imm16
                imm16 = read(pc++); imm16 |= (read(pc++) << 8);
                write(--sp, (pc & 0xff00) >> 8);
                write(--sp, pc & 0x00ff);
                pc = imm16;
                
                r8 = read(sp+1), r8_2 = read(sp);
                if (debug) {printf("call imm16"); printf("\n");}
                return 6;
            case 0xe2:  // ldh [c], a
                write(0xff00 | readR8(1), readR8(7));

                if (debug) {printf("ldh [c], a"); printf("\n");}
                return 2;
            case 0xe0:  // ldh [imm8], a
                imm8 = read(pc++);
                write(0xff00 | imm8, readR8(7));

                if (debug) {printf("ldh [imm8], a"); printf("\n");}
                return 3;
            case 0xea:  // ld [imm16], a
                imm16 = read(pc++); imm16 |= (read(pc++) << 8);
                write(imm16, readR8(7));
                
                if (debug) {printf("ld [imm16], a"); printf("\n");}
                return 4;
            case 0xf2:  // ldh a, [c]
                writeR8(7, read(0xff00 | readR8(1)));

                if (debug) {printf("ldh a, [c]"); printf("\n");}
                return 2;
            case 0xf0:  // ldh a, [imm8]
                imm8 = read(pc++);
                writeR8(7, read(0xff00 | imm8));

                if (debug) {printf("ldh a, [imm8] = %02X", imm8); printf("\n");}
                return 3;
            case 0xfa:  // ld a, [imm16]
                imm16 = read(pc++); imm16 |= (read(pc++) << 8);
                writeR8(7, read(imm16));

                if (debug) {printf("ld a, [imm16]"); printf("\n");}
                return 4;
            case 0xe8:  // add sp, imm8
                imm8_signed = read(pc++);
                setCarryFlag(causesAddOverflow((uint8_t)(sp & 0xff), imm8_signed));
                setHalfCarryFlag(causesHalfAddOverflow((uint8_t)(sp & 0xff), imm8_signed));
                setSubtractionFlag(0);
                setZeroFlag(0);
                sp = sp + imm8_signed;

                if (debug) {printf("add sp, imm8"); printf("\n");}
                return 4;
            case 0xf8:  // ld hl, sp + imm8
                imm8_signed = read(pc++);
                setHalfCarryFlag((((sp & 0xf) + (imm8_signed & 0xf)) & 0x10) > 0);
                setCarryFlag((sp & 0xff) + (uint8_t)imm8_signed > 0xff);
                setZeroFlag(0);
                setSubtractionFlag(0);
                hl = sp + imm8_signed;

                if (debug) {printf("ld hl, sp + imm8"); printf("\n");}
                return 3;
            case 0xf9:  // ld sp, hl
                sp = hl;
                
                if (debug) {printf("ld sp, hl"); printf("\n");}
                return 2;
            case 0xf3:  // di
                // clear IME flag
                IME = false;

                if (debug) {printf("di"); printf("\n");}
                return 1;
            case 0xfb:  // ei
                // set IME flag AFTER NEXT INSTRUCTION
                ei_timer = 2;

                if (debug) {printf("ei"); printf("\n");}
                return 1;
            case 0xcb:  // prefix ($CB)
                op = read(pc++);
                // if (debug) printf("%02X ", op);
                switch ((op & 0xc0) >> 6) {
                    case 0x00:
                        r8 = op & 0x07;
                        switch ((op & 0x38) >> 3) {
                            case 0x00:  // rlc r8
                                setCarryFlag((readR8(r8) & 0x80) > 0);
                                writeR8(r8, ((readR8(r8) & 0x7f) << 1) | (readR8(r8) & 0x80) >> 7);
                                setZeroFlag(readR8(r8) == 0);
                                setSubtractionFlag(0);
                                setHalfCarryFlag(0);

                                if (debug) {printf("rlc "); printR8(r8); printf("\n");}
                                return 2 + (r8 == 6) * 2;
                            case 0x01:  // rrc r8
                                setCarryFlag((readR8(r8) & 0x01) > 0);
                                writeR8(r8, ((readR8(r8) & 0xfe) >> 1) | ((readR8(r8) & 0x01) << 7));
                                setZeroFlag(readR8(r8) == 0);
                                setSubtractionFlag(0);
                                setHalfCarryFlag(0);

                                if (debug) {printf("rrc "); printR8(r8); printf("\n");}
                                return 2 + (r8 == 6) * 2;
                            case 0x02:  //  rl r8
                                carry = getCarryFlag();
                                setCarryFlag((readR8(r8) & 0x80) > 0);
                                writeR8(r8, ((readR8(r8) & 0x7f) << 1) | carry);
                                setZeroFlag(readR8(r8) == 0);
                                setSubtractionFlag(0);
                                setHalfCarryFlag(0);

                                if (debug) {printf("rl "); printR8(r8); printf("\n");}
                                return 2 + (r8 == 6) * 2;
                            case 0x03:  //  rr r8
                                carry = getCarryFlag();
                                setCarryFlag((readR8(r8) & 0x01) > 0);
                                writeR8(r8, ((readR8(r8) & 0xfe) >> 1) | (carry << 7));
                                setZeroFlag(readR8(r8) == 0);
                                setSubtractionFlag(0);
                                setHalfCarryFlag(0);

                                if (debug) {printf("rr "); printR8(r8); printf("\n");}
                                return 2 + (r8 == 6) * 2;
                            case 0x04:  // sla r8
                                setCarryFlag((readR8(r8) & 0x80) > 0);
                                writeR8(r8, (readR8(r8) & 0x7f) << 1);
                                setZeroFlag(readR8(r8) == 0);
                                setSubtractionFlag(0);
                                setHalfCarryFlag(0);

                                if (debug) {printf("sla "); printR8(r8); printf("\n");}
                                return 2 + (r8 == 6) * 2;
                            case 0x05:  // sra r8
                                setCarryFlag((readR8(r8) & 0x01) > 0);
                                writeR8(r8, ((readR8(r8) & 0xfe) >> 1) | (readR8(r8) & 0x80));
                                setZeroFlag(readR8(r8) == 0);
                                setSubtractionFlag(0);
                                setHalfCarryFlag(0);

                                if (debug) {printf("sra "); printR8(r8); printf("\n");}
                                return 2 + (r8 == 6) * 2;
                            case 0x06:  // swap r8
                                writeR8(r8, ((readR8(r8) & 0xf0) >> 4) | ((readR8(r8) & 0x0f) << 4));
                                setZeroFlag(readR8(r8) == 0);
                                setSubtractionFlag(0);
                                setHalfCarryFlag(0);
                                setCarryFlag(0);

                                if (debug) {printf("swap "); printR8(r8); printf("\n");}
                                return 2 + (r8 == 6) * 2;
                            case 0x07:  // srl r8
                                setCarryFlag((readR8(r8) & 0x01) > 0);
                                writeR8(r8, (readR8(r8) & 0xfe) >> 1);
                                setZeroFlag(readR8(r8) == 0);
                                setSubtractionFlag(0);
                                setHalfCarryFlag(0);

                                if (debug) {printf("srl "); printR8(r8); printf("\n");}
                                return 2 + (r8 == 6) * 2;
                        }
                    case 0x01:  // bit b3, r8
                        u3 = (op & 0x38) >> 3;
                        r8 = op & 0x7;
                        setZeroFlag(!(readR8(r8) & (1 << u3)));
                        setSubtractionFlag(0);
                        setHalfCarryFlag(1);
                        
                        if (debug) {printf("bit %d, ", u3); printR8(r8); printf("\n");}
                        return 2 + (r8 == 6);
                    case 0x02:  // res b3, r8
                        u3 = (op & 0x38) >> 3;
                        r8 = op & 0x7;
                        writeR8(r8, readR8(r8) & (~(1 << u3)));
                        
                        if (debug) {printf("res %d, ", u3); printR8(r8); printf("\n");}
                        return 2 + (r8 == 6) * 2;
                    case 0x03:  // set b3, r8
                        u3 = (op & 0x38) >> 3;
                        r8 = op & 0x7;
                        writeR8(r8, readR8(r8) | (1 << u3));

                        if (debug) {printf("set %d, ", u3); printR8(r8); printf("\n");}
                        return 2 + (r8 == 6) * 2;
                }
                break;
            default:
                if ((op & 0x27) == 0x00) {  // ret cond
                    cc = (op & 0x18) >> 3;

                    if (debug) {printf("ret "); printCond(cc); printf("\n");}
                    if (getCallCondition(cc)) {
                        r8 = read(sp++);
                        r8_2 = read(sp++);
                        pc = r8 | (r8_2 << 8);

                        return 5;
                    }
                    return 2;
                } else if ((op & 0x27) == 0x02) {   // jp cond, imm16
                    imm16 = read(pc++); imm16 |= (read(pc++) << 8);
                    cc = (op & 0x18) >> 3;

                    if (debug) {printf("jp "); printCond(cc); printf(", imm16\n");}
                    if (getCallCondition(cc)) {
                        pc = imm16;
                        return 4;
                    }
                    return 3;
                } else if ((op & 0x27) == 0x04) {   // call cond, imm16
                    imm16 = read(pc++); imm16 |= (read(pc++) << 8);
                    cc = (op & 0x18) >> 3;

                    if (debug) {printf("call "); printCond(cc); printf(", imm16\n");}
                    if (getCallCondition(cc)) {
                        write(--sp, (pc & 0xff00) >> 8);
                        write(--sp, pc & 0x00ff);
                        pc = imm16;
                        return 6;
                    }
                    return 3;
                } else if ((op & 0x07) == 0x07) {   // rst tgt3
                    imm16 = (op & 0x38);
                    write(--sp, (pc & 0xff00) >> 8);
                    write(--sp, pc & 0x00ff);
                    pc = imm16;
                    
                    if (debug) {printf("rst %02X", imm16); printf("\n");}
                    return 4;
                } else if ((op & 0x0f) == 0x01) {   // pop r16stk
                    r16 = (op & 0x30) >> 4;
                    r8 = read(sp++);
                    r8_2 = read(sp++);
                    if (r16 == 3) af = (r8 | (r8_2 << 8)) & 0xfff0;
                    else writeR16(r16, r8 | (r8_2 << 8));

                    if (debug) {printf("pop "); printR16stk(r16); printf("\n");}
                    return 3;
                } else if ((op & 0x0f) == 0x05) {   // push r16stk
                    r16 = (op & 0x30) >> 4;
                    if (r16 == 3) {
                        write(--sp, (af & 0xff00) >> 8);
                        write(--sp, af & 0x00ff);
                    }
                    else {
                        write(--sp, (readR16(r16) & 0xff00) >> 8);
                        write(--sp, readR16(r16) & 0x00ff);
                    }

                    if (debug) {printf("push "); printR16stk(r16); printf("\n");}
                    return 4;
                }
                break;
        }
        break;
    }


    if (debug) printf("unknown: %02X\n", op);
    return 0;
}

void CPU::startupCircumvention() {
    if (color_on) {
        af = 0x1180;
        bc = 0x0000;
        de = 0xFF56;
        hl = 0x000D;
        pc = 0x0100;
        sp = 0xFFFE;

        write(0xff00, 0xcf);
        write(0xff01, 0x00);
        write(0xff02, 0x7f);
        write(0xff04, 0x00);
        write(0xff05, 0x00);
        write(0xff06, 0x00);
        write(0xff07, 0xf8);
        write(0xff0f, 0xe1);
        write(0xff10, 0x80);
        write(0xff11, 0xbf);
        write(0xff12, 0xf3);
        write(0xff13, 0xff);
        write(0xff14, 0xbf);
        write(0xff16, 0x3f);
        write(0xff17, 0x00);
        write(0xff18, 0xff);
        write(0xff19, 0xbf);
        write(0xff1a, 0x7f);
        write(0xff1b, 0xff);
        write(0xff1c, 0x9f);
        write(0xff1d, 0xff);
        write(0xff1e, 0xbf);
        write(0xff20, 0xff);
        write(0xff21, 0x00);
        write(0xff22, 0x00);
        write(0xff23, 0xbf);
        write(0xff24, 0x77);
        write(0xff25, 0xf3);
        write(0xff26, 0xf1);
        write(0xff40, 0x91);
        write(0xff41, 0x00);
        write(0xff42, 0x00);
        write(0xff43, 0x00);
        write(0xff44, 0x00);
        write(0xff45, 0x00);
        write(0xff46, 0x00);
        write(0xff47, 0xfc);
        write(0xff48, 0x00);
        write(0xff49, 0x00);
        write(0xff4a, 0x00);
        write(0xff4b, 0x00);
        write(0xff4d, 0x7e);
        write(0xff4f, 0xfe);
        write(0xff51, 0xff);
        write(0xff52, 0xff);
        write(0xff53, 0xff);
        write(0xff54, 0xff);
        write(0xff55, 0xff);
        write(0xff56, 0x3e);
        write(0xff68, 0x00);
        write(0xff69, 0x00);
        write(0xff6a, 0x00);
        write(0xff6b, 0x00);
        write(0xff70, 0xf8);
        write(0xffff, 0x00);
    }
    else {
        af = 0x0100;
        bc = 0xFF13;
        de = 0x00C1;
        hl = 0x8403;
        pc = 0x0100;
        sp = 0xFFFE;

        write(0xff00, 0xcf);
        write(0xff01, 0x00);
        write(0xff02, 0x7e);
        write(0xff04, 0xab);
        write(0xff05, 0x00);
        write(0xff06, 0x00);
        write(0xff07, 0xf8);
        write(0xff0f, 0xe1);
        write(0xff10, 0x80);
        write(0xff11, 0xbf);
        write(0xff12, 0xf3);
        write(0xff13, 0xff);
        write(0xff14, 0xbf);
        write(0xff16, 0x3f);
        write(0xff17, 0x00);
        write(0xff18, 0xff);
        write(0xff19, 0xbf);
        write(0xff1a, 0x7f);
        write(0xff1b, 0xff);
        write(0xff1c, 0x9f);
        write(0xff1d, 0xff);
        write(0xff1e, 0xbf);
        write(0xff20, 0xff);
        write(0xff21, 0x00);
        write(0xff22, 0x00);
        write(0xff23, 0xbf);
        write(0xff24, 0x77);
        write(0xff25, 0xf3);
        write(0xff26, 0xf1);
        write(0xff40, 0x91);
        write(0xff41, 0x85);
        write(0xff42, 0x00);
        write(0xff43, 0x00);
        write(0xff44, 0x00);
        write(0xff45, 0x00);
        write(0xff46, 0xff);
        write(0xff47, 0xfc);
        write(0xff48, 0x00);
        write(0xff49, 0x00);
        write(0xff4a, 0x00);
        write(0xff4b, 0x00);
        write(0xffff, 0x00);
    }
}