/**
 * @file bme280.c
 * @brief BME280 driver core.
 *
 * Register access, init sequence, configuration, calibration parsing and the
 * integer compensation formulas of the datasheet (section 4.2.3).
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

/** Little endian 16-bit word (LSB first, as in the calibration block). */
static uint16_t le_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static int16_t le_s16(const uint8_t *p)
{
    return (int16_t)le_u16(p);
}

/* ------------------------------------------------------------------------- */
/* Low level register access                                                 */
/* ------------------------------------------------------------------------- */

bme280_status_t bme280_read_regs(const bme280_dev_t *dev, uint8_t reg, uint8_t *buf, size_t len)
{
    if ((dev == NULL) || (buf == NULL) || (dev->hal == NULL) || (dev->hal->i2c_read == NULL)) {
        return BME280_E_NULL_PTR;
    }
    if (len == 0U) {
        return BME280_E_INVALID_ARG;
    }
    if (dev->hal->i2c_read(dev->hal->ctx, dev->addr, reg, buf, len) != 0) {
        return BME280_E_COMM;
    }
    return BME280_OK;
}

bme280_status_t bme280_write_reg(const bme280_dev_t *dev, uint8_t reg, uint8_t value)
{
    if ((dev == NULL) || (dev->hal == NULL) || (dev->hal->i2c_write == NULL)) {
        return BME280_E_NULL_PTR;
    }
    if (dev->hal->i2c_write(dev->hal->ctx, dev->addr, reg, &value, 1U) != 0) {
        return BME280_E_COMM;
    }
    return BME280_OK;
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
    if ((tp == NULL) || (h == NULL) || (out == NULL)) {
        return BME280_E_NULL_PTR;
    }

    /* tp[] = registers 0x88..0xA1 (index = register - 0x88) */
    out->dig_T1 = le_u16(&tp[0]);
    out->dig_T2 = le_s16(&tp[2]);
    out->dig_T3 = le_s16(&tp[4]);
    out->dig_P1 = le_u16(&tp[6]);
    out->dig_P2 = le_s16(&tp[8]);
    out->dig_P3 = le_s16(&tp[10]);
    out->dig_P4 = le_s16(&tp[12]);
    out->dig_P5 = le_s16(&tp[14]);
    out->dig_P6 = le_s16(&tp[16]);
    out->dig_P7 = le_s16(&tp[18]);
    out->dig_P8 = le_s16(&tp[20]);
    out->dig_P9 = le_s16(&tp[22]);
    out->dig_H1 = tp[25];

    /* h[] = registers 0xE1..0xE7 (index = register - 0xE1) */
    out->dig_H2 = le_s16(&h[0]);
    out->dig_H3 = h[2];
    /* H4 = 0xE4[7:0] (signed MSB) : 0xE5[3:0], H5 = 0xE6[7:0] (signed MSB) : 0xE5[7:4].
     * Multiply instead of shifting so a negative MSB is not left shifted. */
    out->dig_H4 = (int16_t)(((int32_t)(int8_t)h[3] * 16) + (int32_t)(h[4] & 0x0FU));
    out->dig_H5 = (int16_t)(((int32_t)(int8_t)h[5] * 16) + (int32_t)(h[4] >> 4));
    out->dig_H6 = (int8_t)h[6];
    return BME280_OK;
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
    int32_t var1;
    int32_t var2;
    int32_t fine;

    if (calib == NULL) {
        return 0;
    }
    var1 = (((adc_T >> 3) - ((int32_t)calib->dig_T1 * 2)) * (int32_t)calib->dig_T2) >> 11;
    var2 = (((((adc_T >> 4) - (int32_t)calib->dig_T1) * ((adc_T >> 4) - (int32_t)calib->dig_T1)) >>
             12) *
            (int32_t)calib->dig_T3) >>
           14;
    fine = var1 + var2;
    if (t_fine != NULL) {
        *t_fine = fine;
    }
    return ((fine * 5) + 128) >> 8;
}

uint32_t bme280_compensate_pressure(const bme280_calib_t *calib, int32_t adc_P, int32_t t_fine)
{
    int64_t var1;
    int64_t var2;
    int64_t p;

    if (calib == NULL) {
        return 0U;
    }
    /* Datasheet left shifts of possibly negative values are written as
     * multiplications by the same power of two (shifting a negative is UB). */
    var1 = (int64_t)t_fine - 128000;
    var2 = var1 * var1 * (int64_t)calib->dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib->dig_P5) * 131072);      /* << 17 */
    var2 = var2 + ((int64_t)calib->dig_P4 * INT64_C(34359738368)); /* << 35 */
    var1 = ((var1 * var1 * (int64_t)calib->dig_P3) >> 8) +
           ((var1 * (int64_t)calib->dig_P2) * 4096);                           /* << 12 */
    var1 = ((INT64_C(140737488355328) + var1) * (int64_t)calib->dig_P1) >> 33; /* 1 << 47 */
    if (var1 == 0) {
        return 0U;
    }
    p = 1048576 - (int64_t)adc_P;
    p = (((p * INT64_C(2147483648)) - var2) * 3125) / var1; /* << 31 */
    var1 = ((int64_t)calib->dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)calib->dig_P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)calib->dig_P7 * 16); /* << 4 */
    return (uint32_t)p;
}

uint32_t bme280_compensate_humidity(const bme280_calib_t *calib, int32_t adc_H, int32_t t_fine)
{
    int32_t v;

    if (calib == NULL) {
        return 0U;
    }
    v = t_fine - 76800;
    v = (((((adc_H * 16384) - ((int32_t)calib->dig_H4 * 1048576) - ((int32_t)calib->dig_H5 * v)) +
           16384) >>
          15) *
         (((((((v * (int32_t)calib->dig_H6) >> 10) *
              (((v * (int32_t)calib->dig_H3) >> 11) + 32768)) >>
             10) +
            2097152) *
               (int32_t)calib->dig_H2 +
           8192) >>
          14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)calib->dig_H1) >> 4);
    if (v < 0) {
        v = 0;
    }
    if (v > 419430400) {
        v = 419430400;
    }
    return (uint32_t)(v >> 12);
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
