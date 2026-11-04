/**
 * @file fake_i2c.c
 * @brief In-memory fake BME280 I2C bus for host unit tests.
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#include "fake_i2c.h"

#include <string.h>

#include "bme280_regs.h"

/*
 * Temperature and pressure calibration from the worked example published by
 * Bosch Sensortec for the BMP280 (same T/P registers and formulas as the BME280):
 *   dig_T1 = 27504, dig_T2 = 26435, dig_T3 = -1000,
 *   dig_P1 = 36477, dig_P2 = -10685, dig_P3 = 3024, dig_P4 = 2855, dig_P5 = 140,
 *   dig_P6 = -7, dig_P7 = 15500, dig_P8 = -14600, dig_P9 = 6000
 * plus a humidity set typical of a real BME280:
 *   dig_H1 = 75, dig_H2 = 362, dig_H3 = 0, dig_H4 = 313, dig_H5 = 50, dig_H6 = 30
 */
const uint8_t FIXTURE_CALIB_TP[26] = {
    0x70, 0x6B, /* 0x88 dig_T1 = 27504  */
    0x43, 0x67, /* 0x8A dig_T2 = 26435  */
    0x18, 0xFC, /* 0x8C dig_T3 = -1000  */
    0x7D, 0x8E, /* 0x8E dig_P1 = 36477  */
    0x43, 0xD6, /* 0x90 dig_P2 = -10685 */
    0xD0, 0x0B, /* 0x92 dig_P3 = 3024   */
    0x27, 0x0B, /* 0x94 dig_P4 = 2855   */
    0x8C, 0x00, /* 0x96 dig_P5 = 140    */
    0xF9, 0xFF, /* 0x98 dig_P6 = -7     */
    0x8C, 0x3C, /* 0x9A dig_P7 = 15500  */
    0xF8, 0xC6, /* 0x9C dig_P8 = -14600 */
    0x70, 0x17, /* 0x9E dig_P9 = 6000   */
    0x00,       /* 0xA0 reserved        */
    0x4B,       /* 0xA1 dig_H1 = 75     */
};

const uint8_t FIXTURE_CALIB_H[7] = {
    0x6A, 0x01, /* 0xE1 dig_H2 = 362                     */
    0x00,       /* 0xE3 dig_H3 = 0                       */
    0x13,       /* 0xE4 dig_H4[11:4]                     */
    0x29,       /* 0xE5 dig_H5[3:0] = 2 | dig_H4[3:0] = 9 */
    0x03,       /* 0xE6 dig_H5[11:4]  -> H4 = 313, H5 = 50 */
    0x1E,       /* 0xE7 dig_H6 = 30                      */
};

/* adc_P = 415148 (0x655AC), adc_T = 519888 (0x7EED0), adc_H = 30000 (0x7530) */
const uint8_t FIXTURE_DATA[8] = {0x65, 0x5A, 0xC0, 0x7E, 0xED, 0x00, 0x75, 0x30};

void fake_i2c_reset(fake_i2c_t *bus, uint8_t dev_addr)
{
    (void)memset(bus, 0, sizeof(*bus));
    bus->dev_addr = dev_addr;
}

void fake_i2c_load(fake_i2c_t *bus, uint8_t reg, const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0U; i < len; i++) {
        bus->regs[(uint8_t)(reg + i)] = data[i];
    }
}

void fake_i2c_load_bme280_fixture(fake_i2c_t *bus)
{
    bus->regs[BME280_REG_CHIP_ID] = BME280_CHIP_ID_VALUE;
    fake_i2c_load(bus, BME280_REG_CALIB_TP_START, FIXTURE_CALIB_TP, sizeof(FIXTURE_CALIB_TP));
    fake_i2c_load(bus, BME280_REG_CALIB_H_START, FIXTURE_CALIB_H, sizeof(FIXTURE_CALIB_H));
    fake_i2c_load(bus, BME280_REG_DATA, FIXTURE_DATA, sizeof(FIXTURE_DATA));
}

int fake_i2c_read(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t *data, size_t len)
{
    fake_i2c_t *bus = (fake_i2c_t *)ctx;
    size_t i;

    if ((bus == NULL) || (data == NULL)) {
        return -1;
    }
    bus->read_calls++;
    bus->last_read_reg = reg;
    bus->last_read_len = len;
    if (bus->fail_reads || (bus->fail_read_at == (int)bus->read_calls)) {
        return -1;
    }
    if (dev_addr != bus->dev_addr) {
        return -2; /* NACK */
    }
    if (((size_t)reg + len) > sizeof(bus->regs)) {
        return -3;
    }
    for (i = 0U; i < len; i++) {
        data[i] = bus->regs[reg + i]; /* auto-increment */
    }
    return 0;
}

int fake_i2c_write(void *ctx, uint8_t dev_addr, uint8_t reg, const uint8_t *data, size_t len)
{
    fake_i2c_t *bus = (fake_i2c_t *)ctx;
    size_t i;

    if ((bus == NULL) || (data == NULL)) {
        return -1;
    }
    bus->write_calls++;
    if (bus->fail_writes) {
        return -1;
    }
    if (dev_addr != bus->dev_addr) {
        return -2; /* NACK */
    }
    for (i = 0U; i < len; i++) {
        uint8_t r = (uint8_t)(reg + i);
        if (bus->write_count < FAKE_I2C_MAX_WRITES) {
            bus->writes[bus->write_count].reg = r;
            bus->writes[bus->write_count].value = data[i];
            bus->write_count++;
        }
        /* The reset register always reads 0x00, the command is not stored. */
        if (r != BME280_REG_RESET) {
            bus->regs[r] = data[i];
        }
    }
    return 0;
}

void fake_i2c_delay_ms(void *ctx, uint32_t ms)
{
    fake_i2c_t *bus = (fake_i2c_t *)ctx;
    if (bus != NULL) {
        bus->delay_calls++;
        bus->total_delay_ms += ms;
    }
}

bme280_hal_t fake_i2c_hal(fake_i2c_t *bus)
{
    bme280_hal_t hal;
    hal.i2c_read = fake_i2c_read;
    hal.i2c_write = fake_i2c_write;
    hal.delay_ms = fake_i2c_delay_ms;
    hal.ctx = bus;
    return hal;
}
