#pragma once

#include "../config/rv32_config.h"

#include "pico/stdlib.h"
#include "hardware/i2c.h"

void RTCInit();
uint32_t readTime();
void setTime(uint32_t time);