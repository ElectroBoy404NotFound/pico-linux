#include "rtc.h"

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "../config/rv32_config.h"

int reg_write(i2c_inst_t *i2c, const uint addr, const uint8_t reg, uint8_t *buf, const uint8_t nbytes) {
    int num_bytes_read = 0;
    uint8_t msg[nbytes + 1];

    if (nbytes < 1)
        return 0;

    msg[0] = reg;
    for (int i = 0; i < nbytes; i++)
        msg[i + 1] = buf[i];

    i2c_write_blocking(i2c, addr, msg, (nbytes + 1), false);

    return num_bytes_read;
}

int reg_read( i2c_inst_t *i2c, const uint addr, const uint8_t reg, uint8_t *buf, const uint8_t nbytes) {
    int num_bytes_read = 0;

    if (nbytes < 1)
        return 0;

    i2c_write_blocking(i2c, addr, &reg, 1, true);
    num_bytes_read = i2c_read_blocking(i2c, addr, buf, nbytes, false);

    return num_bytes_read;
}

void RTCInit() {
    i2c_init(DS3231_I2C_PORT, 400 * 1000);

    gpio_set_function(DS3231_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(DS3231_I2C_SCL, GPIO_FUNC_I2C);

    gpio_pull_up(DS3231_I2C_SDA);
    gpio_pull_up(DS3231_I2C_SCL);
}

uint32_t readTime() {
    uint32_t time = 0;
    uint8_t buf = 0;
    
    // Seconds Register
    reg_read(DS3231_I2C_PORT, 0x68, 0, &buf, 1);
    time += (buf & 0xf) + (10*(buf>>4));

    // Minutes Register
    reg_read(DS3231_I2C_PORT, 0x68, 1, &buf, 1);
    time += 60*((buf & 0xf) + (10*(buf>>4)));

    // Hours register
    reg_read(DS3231_I2C_PORT, 0x68, 1, &buf, 1);
    if((buf >> 6) & 1) 
        if((buf >> 5) & 1) time += (60*(60*((buf & 0xf) + (10*(buf>>4))))) + (12*60*60);
        else time += 60*(60*((buf & 0xf) + (10*(buf>>4))));
    else time += 60*(60*((buf & 0xf) + (10*(buf>>4))));
}