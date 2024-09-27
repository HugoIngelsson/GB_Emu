#include "ROM.h"

ROM::ROM(bool color_on) {
    string name;
    if (color_on)
        name = cgb_boot_name;
    else
        name = dmg_boot_name;

    // read boot rom
    ifstream f(name, ios::in|ios::binary|ios::ate);
    if (!f.good()) {
        cout << "Couldn't read '" << cgb_boot_name << "'!" << endl;
    }

    // file size
    int fsize = f.tellg();
    f.seekg(ios::beg);
    printf("Boot ROM size: %04X\n", fsize);

    boot_rom = (uint8_t*)malloc(fsize * sizeof(uint8_t));
    f.read((char*)boot_rom, fsize);
    f.close();
}

ROM::~ROM() {
    saveFile();

    if (ROM_data != NULL) {
        free(ROM_data);
    }

    if (RAM_data != NULL) {
        free(RAM_data);
    }
}

int ROM::load(string file) {
    file_name = file;
    if (file.find('/') == file.npos && file.find('\\') == file.npos) {
        file_path = "";
        file_raw_name = file;
    } else {
        file_path = file.substr(0, file.find_last_of("/\\"));
        file_raw_name = file.substr(file.find_last_of("/\\")+1);
    }

    // read file
    ifstream f(file, ios::in|ios::binary|ios::ate);
    if (!f.good()) {
        cout << "Couldn't read '" << file << "'!" << endl;
        return 1;
    }

    // file size
    int fsize = f.tellg();
    f.seekg(ios::beg);

    ROM_data = (uint8_t*)malloc(fsize * sizeof(uint8_t));
    if (ROM_data == NULL) {
        cout << "Out of memory! File is too big." << endl;
        return 1;
    }

    f.read((char*)ROM_data, fsize);
    f.close();

    cout << "File read, file size " << fsize << "." << endl;

    nintendo_logo =        ROM_data + 0x0104;
    title =                ROM_data + 0x0134;
    manu_code =            ROM_data + 0x013F;
    CGB_flag =             ROM_data + 0x0143;
    new_licensee_code =    ROM_data + 0x0144;
    SGB_flag =             ROM_data + 0x0146;
    cart_type =            ROM_data + 0x0147;
    ROM_size_info =        ROM_data + 0x0148;
    RAM_size_info =        ROM_data + 0x0149;
    dest_code =            ROM_data + 0x014A;
    old_licensee_code =    ROM_data + 0x014B;
    mask_ROM_v =           ROM_data + 0x014C;
    header_checksum =      ROM_data + 0x014D;
    global_checksum =      ROM_data + 0x014E;

    if (*ROM_size_info <= 0x08) {
        ROM_size = 0x8000 * (1 << *ROM_size_info);
        num_ROM_banks = (1 << *ROM_size_info) * 2;
    }
    else {
        cout << "Unsupported ROM size." << endl;
        return 1;
    }

    switch (*RAM_size_info) {
        case 0x00: RAM_size = 0;
        case 0x01: break;
        case 0x02: RAM_size = 0x2000; break;
        case 0x03: RAM_size = 0x8000; break;
        case 0x04: RAM_size = 0x20000; break;
        case 0x05: RAM_size = 0x10000; break;
    }
    num_RAM_banks = RAM_size / 0x2000;

    RAM_data = (uint8_t*)calloc(RAM_size, sizeof(uint8_t));
    if (RAM_data == NULL) {
        cout << "Out of memory! Not enough space for RAM." << endl;
        return 1;
    }

    ifstream saved_ram(file_path + "/saves/" + file_raw_name + ".extram", ios::in|ios::binary|ios::ate);
    if (saved_ram.good()) {
        fsize = saved_ram.tellg();
        saved_ram.seekg(ios::beg);

        if (fsize == RAM_size) {
            saved_ram.read((char*)RAM_data, RAM_size);
            printf("External RAM loaded\n");
        }
    }

    if (*cart_type >= 0x1 && *cart_type <= 0x3)
        MBC1 = true;
    else if (*cart_type >= 0x0F && *cart_type <= 0x13)
        MBC3 = true;
    else if (*cart_type >= 0x19 && *cart_type <= 0x1E)
        MBC5 = true;

    return 0;
}

void ROM::printROMinfo() {
    for (uint8_t r=0; r<3; r++) { for (uint8_t i=0; i<16; i++) 
        {
            printf("%02X ", *(nintendo_logo+16*r+i));
        }

        printf("\n");
    }

    for (int i=0; i<16 && *(title+i) != 0x00; i++) {
        printf("%c", *(title+i));
    }
    printf("\n");

    printf("%c%c%c%c\n", *(manu_code), *(manu_code+1), *(manu_code+2), *(manu_code+3));
    printf("%02X\n", *(CGB_flag));
    printf("%c%c\n", *(new_licensee_code), *(new_licensee_code+1));
    printf("%02X\n", *(SGB_flag));
    printf("%02X = ", *(cart_type)); cout << cartridge_types[*cart_type] << endl;
    printf("%02X = %d bits of ROM, %d banks\n", *(ROM_size_info), ROM_size, num_ROM_banks);
    printf("%02X = %d bits of RAM, %d banks\n", *(RAM_size_info), RAM_size, num_RAM_banks);

    if (*dest_code == 0x00)
        cout << "Destination: Japan" << endl;
    else
        cout << "Destination: Overseas only" << endl;

    printf("%02X\n", *(old_licensee_code));
    printf("%02X\n", *(mask_ROM_v));
    printf("%02X\n", *(header_checksum));
    printf("%02X\n", *(global_checksum));

    printf("%p %p\n", ROM_data, RAM_data);
}

void ROM::saveFile() {
    std::ofstream out(file_path + "/saves/" + file_raw_name + ".extram", std::ofstream::binary);

    if (RAM_data != NULL) {
        out.write((char*)RAM_data, RAM_size);
        printf("External RAM saved\n");
    }

    out.close();
}

uint8_t *ROM::getROMbank(int bank_id) {
    if (bank_id >= num_ROM_banks) {
        cout << "Tried accessing non-existent ROM bank!" << endl;
        return NULL;
    }

    return (ROM_data + bank_id * 0x4000);
}

uint8_t *ROM::getRAMbank(int bank_id) {
    if (bank_id >= num_RAM_banks) {
        cout << "Tried accessing non-existent RAM bank!" << endl;
        return NULL;
    }

    return (RAM_data + bank_id * 0x2000);
}

bool ROM::isCGBGame() {
    return *CGB_flag == 0x80 || *CGB_flag == 0xC0;
}

uint8_t ROM::readBoot(uint16_t mem_address) {
    return *(boot_rom + mem_address);
}