#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

#include "CPU.h"
using namespace std;

class Logger {
public:
    Logger();
    ~Logger();

    FILE *file;
    void open(char const *file_name);
    void close();
    void writeLog(CPU *cpu);
};

#endif