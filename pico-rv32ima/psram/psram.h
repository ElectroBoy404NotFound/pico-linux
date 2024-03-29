#ifndef __PSRAM_H
#define __PSRAM_H

#include "pico/stdlib.h"
#include "../config/rv32_config.h"

void accessPSRAM(uint32_t addr, size_t size, bool write, void *bufP);
int initPSRAM();
void RAMGetStat(uint64_t* reads, uint64_t* writes);

// PSRAM bypass
// #define cache_write(ofs, buf, size) accessPSRAM(ofs, size, true, buf)
// #define cache_read(ofs, buf, size) accessPSRAM(ofs, size, false, buf)

#endif