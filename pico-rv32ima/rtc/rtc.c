#include "rtc.h"

#include <math.h>

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

static uint8_t bcd2bin(uint8_t val) { return val - 6 * (val >> 4); }
static uint8_t bin2bcd(uint8_t val) { return val + 6 * (val / 10); }

const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30};

static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
  if (y >= 2000U)
    y -= 2000U;
  uint16_t days = d;
  for (uint8_t i = 1; i < m; ++i)
    days += daysInMonth[i - 1];
  if (m > 2 && y % 4 == 0)
    ++days;
  return days + 365 * y + (y + 3) / 4 - 1;
}

static uint32_t time2ulong(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
  return ((days * 24UL + h) * 60 + m) * 60 + s;
}

uint32_t unixtime(uint16_t y, uint8_t m, uint8_t d, uint8_t hh, uint8_t mm, uint8_t ss) {
  uint32_t t;
  uint16_t days = date2days(y, m, d);
  t = time2ulong(days, hh, mm, ss);
  t += 946684800;
  return t;
}

uint32_t readTime() {
    uint32_t time = 0;
    uint8_t buf[7];

    reg_read(DS3231_I2C_PORT, DS3231_I2C_ADDR, 0, buf, 7);

    console_printf("GET: Year: %d, Month: %d, Date: %d, Time: %d:%d:%d\r\n", bcd2bin(buf[6]) + 2000U, bcd2bin(buf[5] & 0x7F), bcd2bin(buf[4]), bcd2bin(buf[2]), bcd2bin(buf[1]), bcd2bin(buf[0] & 0x7F));

    return unixtime(bcd2bin(buf[6]) + 2000U, bcd2bin(buf[5] & 0x7F), bcd2bin(buf[4]), bcd2bin(buf[2]), bcd2bin(buf[1]), bcd2bin(buf[0] & 0x7F));
}

static inline int32_t sys_i2c_wbuf(i2c_inst_t* i2c, uint8_t addr, const uint8_t* pBuf, uint32_t len)
{
    return i2c_write_timeout_us(i2c, addr, pBuf, len, false, len * 500);
}

static inline uint8_t numbcd(uint8_t num)
{
    return ((num/10) * 16) + (num % 10);
}

#define LEAPOCH (946684800LL + 86400*(31+29))

#define DAYS_PER_400Y (365*400 + 97)
#define DAYS_PER_100Y (365*100 + 24)
#define DAYS_PER_4Y   (365*4   + 1)

void setTime(uint32_t time) {
    uint64_t days, secs, years;
	uint32_t remdays, remsecs, remyears;
	uint32_t qc_cycles, c_cycles, q_cycles;
	uint32_t months;
	uint32_t wday;
	static const uint8_t days_in_month[] = {31,30,31,30,31,31,30,31,30,31,31,29};

	secs = time - LEAPOCH;
	days = secs / 86400;
	remsecs = secs % 86400;
	if (remsecs < 0) {
		remsecs += 86400;
		days--;
	}

	wday = (3+days)%7;
	if (wday < 0) wday += 7;

	qc_cycles = days / DAYS_PER_400Y;
	remdays = days % DAYS_PER_400Y;
	if (remdays < 0) {
		remdays += DAYS_PER_400Y;
		qc_cycles--;
	}

	c_cycles = remdays / DAYS_PER_100Y;
	if (c_cycles == 4) c_cycles--;
	remdays -= c_cycles * DAYS_PER_100Y;

	q_cycles = remdays / DAYS_PER_4Y;
	if (q_cycles == 25) q_cycles--;
	remdays -= q_cycles * DAYS_PER_4Y;

	remyears = remdays / 365;
	if (remyears == 4) remyears--;
	remdays -= remyears * 365;

	years = remyears + 4*q_cycles + 100*c_cycles + 400LL*qc_cycles;

	for (months=0; days_in_month[months] <= remdays; months++)
		remdays -= days_in_month[months];

	if (months >= 10) {
		months -= 12;
		years++;
	}

    uint8_t dt_buffer[8];
    dt_buffer[0] = 0;
    dt_buffer[1] = numbcd(remsecs % 60);
    dt_buffer[2] = numbcd(remsecs / 60 % 60);
    dt_buffer[3] = numbcd(remsecs / 3600);
    dt_buffer[4] = numbcd(wday + 1);
    dt_buffer[5] = numbcd(remdays + 1);
    dt_buffer[6] = numbcd(months + 2);
    dt_buffer[7] = numbcd((years + 100) - 2000);

    console_printf("SET: Year: %d, Month: %d, Date: %d, Time: %d:%d:%d\r\n", bcd2bin(numbcd((years + 100) - 2000)) + 2000, bcd2bin(numbcd(months + 2)), bcd2bin(numbcd(remdays + 1)), bcd2bin(numbcd(remsecs / 3600)), bcd2bin(numbcd(remsecs / 60 % 60)), bcd2bin(numbcd(remsecs % 60)));

    sys_i2c_wbuf(DS3231_I2C_PORT, DS3231_I2C_ADDR, dt_buffer, sizeof(dt_buffer)) != sizeof(dt_buffer);
}