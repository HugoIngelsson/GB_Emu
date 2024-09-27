#include "Logger.h"

#define FORMAT "A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X\
 L:%02X SP:%04X PC:%04X PCMEM:%02X,%02X,%02X,%02X\n"

Logger::Logger() {
}

Logger::~Logger() {
}

void Logger::open(char const *file_name) {
    file = fopen(file_name, "w");
}

void Logger::writeLog(CPU *cpu) {
    fprintf(file, FORMAT, cpu->readR8(7), cpu->af & 0xff, 
            cpu->readR8(0), cpu->readR8(1), cpu->readR8(2), cpu->readR8(3),
            cpu->readR8(4), cpu->readR8(5), cpu->sp, cpu->pc, 
            cpu->read(cpu->pc), cpu->read(cpu->pc+1), cpu->read(cpu->pc+2), 
            cpu->read(cpu->pc+3));
}

void Logger::close() {
    fclose(file);
}