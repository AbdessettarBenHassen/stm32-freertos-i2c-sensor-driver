/**
 * @file bme280.c
 * @brief BME280 driver core.
 *
 * The glue logic (argument checks, init sequence, configuration, raw data
 * unpacking) is implemented. The functions marked `TODO(Abdessettar)` are
 * intentionally left as stubs: see docs/GUIDE_FR.md for the step-by-step plan.
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#include "bme280.h"

#include <string.h>

/* ------------------------------------------------------------------------- */
/* Private helpers                                                           */
/* ------------------------------------------------------------------------- */

static bool hal_is_valid(const bme280_hal_t *hal)
{
    return (hal != NULL) && (hal->i2c_read != NULL) && (hal->i2c_write != NULL) &&
           (hal->delay_ms != NULL);
}

static bool config_is_valid(const bme280_config_t *cfg)
{
    return (cfg->osr_t <= BME280_OSR_X16) && (cfg->osr_p <= BME280_OSR_X16) &&
           (cfg->osr_h <= BME280_OSR_X16) && (cfg->filter <= BME280_FILTER_16) &&
           (cfg->standby <= BME280_STANDBY_20_MS) &&
           ((cfg->mode == BME280_MODE_SLEEP) || (cfg->mode == BME280_MODE_FORCED) ||
            (cfg->mode == BME280_MODE_NORMAL));
}

/** Oversampling enum to number of samples (0 when skipped). */
static uint32_t osr_to_samples(bme280_osr_t osr)
{
    return (osr == BME280_OSR_SKIP) ? 0U : ((uint32_t)1U << ((uint32_t)osr - 1U));
}

/* ------------------------------------------------------------------------- */
/* Low level register access                                                 */
/* ------------------------------------------------------------------------- */

bme280_status_t bme280_read_regs(const bme280_dev_t *dev, uint8_t reg, uint8_t *buf, size_t len)
{
    /*
     * TODO(Abdessettar): implement the burst register read.
     *  1. Return BME280_E_NULL_PTR if dev, buf, dev->hal or dev->hal->i2c_read is NULL.
     *  2. Return BME280_E_INVALID_ARG if len == 0.
     *  3. Call dev->hal->i2c_read(dev->hal->ctx, dev->addr, reg, buf, len).
     *  4. Map a non-zero HAL return value to BME280_E_COMM, otherwise BME280_OK.
     * Tests: tests/test_bme280_regs.c
     */
    (void)dev;
    (void)reg;
    (void)buf;
    (void)len;
    return BME280_E_NOT_IMPLEMENTED;
}

bme280_status_t bme280_write_reg(const bme280_dev_t *dev, uint8_t reg, uint8_t value)
{
    /*
     * TODO(Abdessettar): implement the single register write.
     *  Same NULL checks as bme280_read_regs() (with i2c_write), then call
     *  dev->hal->i2c_write(...) with a 1-byte buffer and map errors to BME280_E_COMM.
     * Tests: tests/test_bme280_regs.c
     */
    (void)dev;
    (void)reg;
    (void)value;
    return BME280_E_NOT_IMPLEMENTED;
}

/* ------------------------------------------------------------------------- */
/* Initialisation and configuration                                          */
/* ------------------------------------------------------------------------- */

bme280_status_t bme280_read_chip_id(const bme280_dev_t *dev, uint8_t *id)
{
    if (id == NULL) {
        return BME280_E_NULL_PTR;
    }
    return bme280_read_regs(dev, BME280_REG_CHIP_ID, id, 1U);
}

bme280_status_t bme280_soft_reset(const bme280_dev_t *dev)
{
    bme280_status_t rc = bme280_write_reg(dev, BME280_REG_RESET, BME280_SOFT_RESET_CMD);
    if ((rc == BME280_OK) && (dev->hal->delay_ms != NULL)) {
        dev->hal->delay_ms(dev->hal->ctx, BME280_STARTUP_MS);
    }
    return rc;
}

bme280_status_t bme280_parse_calibration(const uint8_t tp[BME280_CALIB_TP_LEN],
                                         const uint8_t h[BME280_CALIB_H_LEN], bme280_calib_t *out)
{
    /*
     * TODO(Abdessettar): decode the calibration bytes (datasheet table 16).
     *  - Return BME280_E_NULL_PTR if any pointer is NULL.
     *  - tp[] holds registers 0x88..0xA1: index = register - 0x88.
     *    T1..P9 are little endian 16-bit words (LSB first). Mind the
     *    signedness: T1 and P1 are unsigned, the others are signed.
     *  - dig_H1 is at 0xA1, i.e. tp[25].
     *  - h[] holds registers 0xE1..0xE7: index = register - 0xE1.
     *    dig_H4 and dig_H5 are 12-bit values sharing register 0xE5 (nibbles!).
     *  - Return BME280_OK.
     * Tests: tests/test_bme280_calib.c
     */
    (void)tp;
    (void)h;
    (void)out;
    return BME280_E_NOT_IMPLEMENTED;
}

bme280_status_t bme280_read_calibration(bme280_dev_t *dev)
{
    uint8_t tp[BME280_CALIB_TP_LEN];
    uint8_t h[BME280_CALIB_H_LEN];
    bme280_status_t rc;

    if (dev == NULL) {
        return BME280_E_NULL_PTR;
    }
    rc = bme280_read_regs(dev, BME280_REG_CALIB_TP_START, tp, sizeof(tp));
    if (rc != BME280_OK) {
        return rc;
    }
    rc = bme280_read_regs(dev, BME280_REG_CALIB_H_START, h, sizeof(h));
    if (rc != BME280_OK) {
        return rc;
    }
    return bme280_parse_calibration(tp, h, &dev->calib);
}

bme280_status_t bme280_init(bme280_dev_t *dev, const bme280_hal_t *hal, uint8_t addr)
{
    bme280_status_t rc;

    if ((dev == NULL) || !hal_is_valid(hal)) {
        return BME280_E_NULL_PTR;
    }
    if ((addr != BME280_I2C_ADDR_PRIMARY) && (addr != BME280_I2C_ADDR_SECONDARY)) {
        return BME280_E_INVALID_ARG;
    }

    (void)memset(dev, 0, sizeof(*dev));
    dev->hal = hal;
    dev->addr = addr;

    rc = bme280_read_chip_id(dev, &dev->chip_id);
    if (rc != BME280_OK) {
        return rc;
    }
    if (dev->chip_id != BME280_CHIP_ID_VALUE) {
        return BME280_E_CHIP_ID;
    }

    rc = bme280_soft_reset(dev);
    if (rc != BME280_OK) {
        return rc;
    }

    rc = bme280_read_calibration(dev);
    if (rc != BME280_OK) {
        return rc;
    }

    dev->initialized = true;
    return BME280_OK;
}

bme280_status_t bme280_set_config(const bme280_dev_t *dev, const bme280_config_t *cfg)
{
    bme280_status_t rc;
    uint8_t ctrl_hum;
    uint8_t config;
    uint8_t ctrl_meas;

    if ((dev == NULL) || (cfg == NULL)) {
        return BME280_E_NULL_PTR;
    }
    if (!config_is_valid(cfg)) {
        return BME280_E_INVALID_ARG;
    }

    ctrl_hum = (uint8_t)((uint8_t)cfg->osr_h & 0x07U);
    config =
        (uint8_t)((((uint8_t)cfg->standby & 0x07U) << 5) | (((uint8_t)cfg->filter & 0x07U) << 2));
    ctrl_meas = (uint8_t)((((uint8_t)cfg->osr_t & 0x07U) << 5) |
                          (((uint8_t)cfg->osr_p & 0x07U) << 2) | ((uint8_t)cfg->mode & 0x03U));

    /* config is only reliably written in sleep mode: go to sleep first. */
    rc = bme280_write_reg(dev, BME280_REG_CTRL_MEAS, (uint8_t)(ctrl_meas & 0xFCU));
    if (rc == BME280_OK) {
        rc = bme280_write_reg(dev, BME280_REG_CTRL_HUM, ctrl_hum);
    }
    if (rc == BME280_OK) {
        rc = bme280_write_reg(dev, BME280_REG_CONFIG, config);
    }
    if (rc == BME280_OK) {
        rc = bme280_write_reg(dev, BME280_REG_CTRL_MEAS, ctrl_meas);
    }
    return rc;
}

bme280_status_t bme280_set_mode(const bme280_dev_t *dev, bme280_mode_t mode)
{
    uint8_t ctrl_meas = 0U;
    bme280_status_t rc;

    if ((mode != BME280_MODE_SLEEP) && (mode != BME280_MODE_FORCED) &&
        (mode != BME280_MODE_NORMAL)) {
        return BME280_E_INVALID_ARG;
    }
    rc = bme280_read_regs(dev, BME280_REG_CTRL_MEAS, &ctrl_meas, 1U);
    if (rc != BME280_OK) {
        return rc;
    }
    ctrl_meas = (uint8_t)((ctrl_meas & 0xFCU) | ((uint8_t)mode & 0x03U));
    return bme280_write_reg(dev, BME280_REG_CTRL_MEAS, ctrl_meas);
}

uint32_t bme280_measurement_time_ms(const bme280_config_t *cfg)
{
    uint32_t t_us;
    uint32_t n;

    if (cfg == NULL) {
        return 0U;
    }
    /* Datasheet section 9.1: t = 1.25 + 2.3*T + (2.3*P + 0.575) + (2.3*H + 0.575) ms */
    t_us = 1250U + (2300U * osr_to_samples(cfg->osr_t));
    n = osr_to_samples(cfg->osr_p);
    if (n != 0U) {
        t_us += (2300U * n) + 575U;
    }
    n = osr_to_samples(cfg->osr_h);
    if (n != 0U) {
        t_us += (2300U * n) + 575U;
    }
    return (t_us + 999U) / 1000U;
}

/* ------------------------------------------------------------------------- */
/* Measurements                                                              */
/* ------------------------------------------------------------------------- */

bme280_status_t bme280_read_uncompensated(const bme280_dev_t *dev, bme280_uncomp_data_t *raw)
{
    uint8_t buf[BME280_DATA_LEN];
    bme280_status_t rc;

    if (raw == NULL) {
        return BME280_E_NULL_PTR;
    }
    rc = bme280_read_regs(dev, BME280_REG_DATA, buf, sizeof(buf));
    if (rc != BME280_OK) {
        return rc;
    }
    /* press_msb, press_lsb, press_xlsb[7:4], temp_msb, temp_lsb, temp_xlsb[7:4], hum_msb, hum_lsb
     */
    raw->pressure = ((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4);
    raw->temperature = ((uint32_t)buf[3] << 12) | ((uint32_t)buf[4] << 4) | ((uint32_t)buf[5] >> 4);
    raw->humidity = ((uint32_t)buf[6] << 8) | (uint32_t)buf[7];
    return BME280_OK;
}

bme280_status_t bme280_read_data(bme280_dev_t *dev, bme280_data_t *data)
{
    bme280_uncomp_data_t raw;
    bme280_status_t rc;

    if ((dev == NULL) || (data == NULL)) {
        return BME280_E_NULL_PTR;
    }
    if (!dev->initialized) {
        return BME280_E_INVALID_ARG;
    }
    rc = bme280_read_uncompensated(dev, &raw);
    if (rc != BME280_OK) {
        return rc;
    }
    data->temperature =
        bme280_compensate_temperature(&dev->calib, (int32_t)raw.temperature, &dev->t_fine);
    data->pressure = bme280_compensate_pressure(&dev->calib, (int32_t)raw.pressure, dev->t_fine);
    data->humidity = bme280_compensate_humidity(&dev->calib, (int32_t)raw.humidity, dev->t_fine);
    return BME280_OK;
}

/* ------------------------------------------------------------------------- */
/* Compensation                                                              */
/* ------------------------------------------------------------------------- */

int32_t bme280_compensate_temperature(const bme280_calib_t *calib, int32_t adc_T, int32_t *t_fine)
{
    /*
     * TODO(Abdessettar): implement the 32-bit integer temperature formula
     * (datasheet section 4.2.3, function BME280_compensate_T_int32).
     *  - Compute var1, var2 from adc_T and dig_T1..dig_T3.
     *  - t_fine = var1 + var2; store it in *t_fine when t_fine != NULL.
     *  - Return (t_fine * 5 + 128) >> 8   (unit: 0.01 degC).
     *  - Return 0 (and leave *t_fine untouched) if calib is NULL.
     * Tests: tests/test_bme280_compensation.c
     */
    (void)calib;
    (void)adc_T;
    (void)t_fine;
    return 0;
}

uint32_t bme280_compensate_pressure(const bme280_calib_t *calib, int32_t adc_P, int32_t t_fine)
{
    /*
     * TODO(Abdessettar): implement the 64-bit integer pressure formula
     * (datasheet section 4.2.3, function BME280_compensate_P_int64).
     *  - Use int64_t for every intermediate value.
     *  - If the divisor (var1) is 0, return 0 to avoid a division by zero.
     *  - Return 0 if calib is NULL.
     *  - Result is Pa in Q24.8 format.
     * Tests: tests/test_bme280_compensation.c
     */
    (void)calib;
    (void)adc_P;
    (void)t_fine;
    return 0U;
}

uint32_t bme280_compensate_humidity(const bme280_calib_t *calib, int32_t adc_H, int32_t t_fine)
{
    /*
     * TODO(Abdessettar): implement the 32-bit integer humidity formula
     * (datasheet section 4.2.3, function bme280_compensate_H_int32).
     *  - Clamp the intermediate value to [0, 419430400] before the final >> 12.
     *  - Return 0 if calib is NULL.
     *  - Result is %RH in Q22.10 format (102400 = 100 %RH).
     * Tests: tests/test_bme280_compensation.c
     */
    (void)calib;
    (void)adc_H;
    (void)t_fine;
    return 0U;
}

/* ------------------------------------------------------------------------- */
/* Misc                                                                      */
/* ------------------------------------------------------------------------- */

const char *bme280_status_str(bme280_status_t status)
{
    switch (status) {
    case BME280_OK:
        return "OK";
    case BME280_E_NULL_PTR:
        return "NULL pointer";
    case BME280_E_COMM:
        return "I2C communication error";
    case BME280_E_CHIP_ID:
        return "unexpected chip id";
    case BME280_E_INVALID_ARG:
        return "invalid argument";
    case BME280_E_NOT_IMPLEMENTED:
        return "not implemented";
    default:
        return "unknown error";
    }
}
