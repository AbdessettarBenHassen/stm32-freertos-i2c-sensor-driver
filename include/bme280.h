/**
 * @file bme280.h
 * @brief Portable driver for the Bosch Sensortec BME280 (temperature, pressure, humidity) over I2C.
 *
 * Design goals:
 * - **Portable**: no MCU or RTOS dependency, all hardware access goes through ::bme280_hal_t.
 * - **No dynamic allocation**: the caller owns every structure.
 * - **Integer only**: compensation uses the fixed-point formulas of the datasheet
 *   (section 4.2.3 and 8.2), so it runs on MCUs without an FPU.
 * - **Testable**: the pure compensation and parsing functions are exposed and
 *   unit-tested on a PC against a fake I2C bus.
 *
 * Typical usage:
 * @code
 * bme280_dev_t dev;
 * bme280_config_t cfg = BME280_CONFIG_DEFAULT;
 * bme280_data_t data;
 *
 * if (bme280_init(&dev, &my_hal, BME280_I2C_ADDR_PRIMARY) == BME280_OK &&
 *     bme280_set_config(&dev, &cfg) == BME280_OK) {
 *     bme280_read_data(&dev, &data);
 *     // data.temperature = 2508      -> 25.08 degC
 *     // data.pressure    = 25767233  -> 25767233 / 256 = ~100653.25 Pa
 *     // data.humidity    = 56317     -> 56317 / 1024 = 55.0 %RH
 * }
 * @endcode
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#ifndef BME280_H
#define BME280_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bme280_hal.h"
#include "bme280_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Driver version, semantic versioning. */
#define BME280_DRIVER_VERSION "0.1.0-dev"

/* ------------------------------------------------------------------------- */
/* Types                                                                     */
/* ------------------------------------------------------------------------- */

/** @brief Status codes returned by every driver function. */
typedef enum {
    BME280_OK = 0,                 /**< Success. */
    BME280_E_NULL_PTR = -1,        /**< A mandatory pointer argument (or HAL callback) is NULL. */
    BME280_E_COMM = -2,            /**< The HAL reported an I2C error. */
    BME280_E_CHIP_ID = -3,         /**< The chip id register did not read ::BME280_CHIP_ID_VALUE. */
    BME280_E_INVALID_ARG = -4,     /**< An argument is out of range. */
    BME280_E_NOT_IMPLEMENTED = -5, /**< Feature not implemented yet (development stub). */
} bme280_status_t;

/** @brief Oversampling setting for one channel (osrs_x fields, section 5.4.3 to 5.4.5). */
typedef enum {
    BME280_OSR_SKIP = 0, /**< Measurement skipped, output set to 0x80000 (0x8000 for humidity). */
    BME280_OSR_X1 = 1,
    BME280_OSR_X2 = 2,
    BME280_OSR_X4 = 3,
    BME280_OSR_X8 = 4,
    BME280_OSR_X16 = 5,
} bme280_osr_t;

/** @brief Sensor power mode (mode[1:0] of ctrl_meas, section 3.3). */
typedef enum {
    BME280_MODE_SLEEP = 0,  /**< No measurement, lowest power. */
    BME280_MODE_FORCED = 1, /**< One measurement, then back to sleep. */
    BME280_MODE_NORMAL = 3, /**< Continuous measurements separated by the standby time. */
} bme280_mode_t;

/** @brief IIR filter coefficient (filter[2:0] of config, section 3.4.4). */
typedef enum {
    BME280_FILTER_OFF = 0,
    BME280_FILTER_2 = 1,
    BME280_FILTER_4 = 2,
    BME280_FILTER_8 = 3,
    BME280_FILTER_16 = 4,
} bme280_filter_t;

/** @brief Standby time between measurements in normal mode (t_sb[2:0], table 27). */
typedef enum {
    BME280_STANDBY_0_5_MS = 0,
    BME280_STANDBY_62_5_MS = 1,
    BME280_STANDBY_125_MS = 2,
    BME280_STANDBY_250_MS = 3,
    BME280_STANDBY_500_MS = 4,
    BME280_STANDBY_1000_MS = 5,
    BME280_STANDBY_10_MS = 6,
    BME280_STANDBY_20_MS = 7,
} bme280_standby_t;

/** @brief Measurement configuration applied by bme280_set_config(). */
typedef struct {
    bme280_osr_t osr_t;       /**< Temperature oversampling. */
    bme280_osr_t osr_p;       /**< Pressure oversampling. */
    bme280_osr_t osr_h;       /**< Humidity oversampling. */
    bme280_filter_t filter;   /**< IIR filter coefficient. */
    bme280_standby_t standby; /**< Standby time (normal mode only). */
    bme280_mode_t mode;       /**< Power mode written last. */
} bme280_config_t;

/**
 * @brief Recommended "weather monitoring" settings (datasheet section 3.5.1),
 *        adapted to normal mode with a 1 s period.
 */
#define BME280_CONFIG_DEFAULT                                  \
    {BME280_OSR_X1,     BME280_OSR_X1,          BME280_OSR_X1, \
     BME280_FILTER_OFF, BME280_STANDBY_1000_MS, BME280_MODE_NORMAL}

/**
 * @brief Factory calibration ("trimming") parameters, datasheet table 16.
 *
 * Names and signedness follow the datasheet exactly. Read once at init.
 */
typedef struct {
    uint16_t dig_T1; /**< 0x88 / 0x89, unsigned. */
    int16_t dig_T2;  /**< 0x8A / 0x8B. */
    int16_t dig_T3;  /**< 0x8C / 0x8D. */
    uint16_t dig_P1; /**< 0x8E / 0x8F, unsigned. */
    int16_t dig_P2;  /**< 0x90 / 0x91. */
    int16_t dig_P3;  /**< 0x92 / 0x93. */
    int16_t dig_P4;  /**< 0x94 / 0x95. */
    int16_t dig_P5;  /**< 0x96 / 0x97. */
    int16_t dig_P6;  /**< 0x98 / 0x99. */
    int16_t dig_P7;  /**< 0x9A / 0x9B. */
    int16_t dig_P8;  /**< 0x9C / 0x9D. */
    int16_t dig_P9;  /**< 0x9E / 0x9F. */
    uint8_t dig_H1;  /**< 0xA1, unsigned. */
    int16_t dig_H2;  /**< 0xE1 / 0xE2. */
    uint8_t dig_H3;  /**< 0xE3, unsigned. */
    int16_t dig_H4;  /**< 0xE4[7:0] / 0xE5[3:0], 12-bit signed. */
    int16_t dig_H5;  /**< 0xE5[7:4] / 0xE6[7:0], 12-bit signed. */
    int8_t dig_H6;   /**< 0xE7, signed. */
} bme280_calib_t;

/** @brief Raw ADC values read from registers 0xF7..0xFE. */
typedef struct {
    uint32_t pressure;    /**< 20-bit unsigned (up). */
    uint32_t temperature; /**< 20-bit unsigned (ut). */
    uint32_t humidity;    /**< 16-bit unsigned (uh). */
} bme280_uncomp_data_t;

/** @brief Compensated measurement in fixed-point units. */
typedef struct {
    int32_t temperature; /**< Temperature in 0.01 degC (2508 = 25.08 degC). */
    uint32_t pressure;   /**< Pressure in Pa, Q24.8 format (divide by 256 to get Pa). */
    uint32_t humidity;   /**< Relative humidity in %RH, Q22.10 format (divide by 1024). */
} bme280_data_t;

/**
 * @brief Driver instance. Treat the fields as private, initialise with bme280_init().
 */
typedef struct {
    const bme280_hal_t *hal; /**< Platform binding (not owned). */
    uint8_t addr;            /**< 7-bit I2C address. */
    uint8_t chip_id;         /**< Last chip id read. */
    bme280_calib_t calib;    /**< Calibration parameters. */
    int32_t t_fine;          /**< Fine temperature shared by the P and H formulas. */
    bool initialized;        /**< Set by a successful bme280_init(). */
} bme280_dev_t;

/* ------------------------------------------------------------------------- */
/* Low level register access                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief Burst-read @p len registers starting at @p reg.
 *
 * Validates the arguments, then forwards to ::bme280_hal_t::i2c_read.
 *
 * @param[in]  dev  Device with a valid @c hal pointer (initialisation not required).
 * @param[in]  reg  First register address.
 * @param[out] buf  Destination buffer of at least @p len bytes.
 * @param[in]  len  Number of bytes, must be > 0.
 * @retval BME280_OK            Success.
 * @retval BME280_E_NULL_PTR    @p dev, @p buf, @c dev->hal or the read callback is NULL.
 * @retval BME280_E_INVALID_ARG @p len is 0.
 * @retval BME280_E_COMM        The HAL callback returned non-zero.
 */
bme280_status_t bme280_read_regs(const bme280_dev_t *dev, uint8_t reg, uint8_t *buf, size_t len);

/**
 * @brief Write a single register.
 *
 * @param[in] dev   Device with a valid @c hal pointer.
 * @param[in] reg   Register address.
 * @param[in] value Value to write.
 * @retval BME280_OK         Success.
 * @retval BME280_E_NULL_PTR @p dev, @c dev->hal or the write callback is NULL.
 * @retval BME280_E_COMM     The HAL callback returned non-zero.
 */
bme280_status_t bme280_write_reg(const bme280_dev_t *dev, uint8_t reg, uint8_t value);

/* ------------------------------------------------------------------------- */
/* Initialisation and configuration                                          */
/* ------------------------------------------------------------------------- */

/**
 * @brief Probe and initialise the sensor.
 *
 * Sequence: check chip id, soft reset, wait ::BME280_STARTUP_MS, read calibration.
 *
 * @param[out] dev  Driver instance to initialise.
 * @param[in]  hal  Platform binding, all callbacks mandatory, must outlive @p dev.
 * @param[in]  addr ::BME280_I2C_ADDR_PRIMARY or ::BME280_I2C_ADDR_SECONDARY.
 * @retval BME280_OK            Sensor detected and calibration loaded.
 * @retval BME280_E_NULL_PTR    NULL argument or NULL HAL callback.
 * @retval BME280_E_INVALID_ARG @p addr is not 0x76 or 0x77.
 * @retval BME280_E_CHIP_ID     Device answered but is not a BME280 (e.g. BMP280 = 0x58).
 * @retval BME280_E_COMM        I2C error.
 */
bme280_status_t bme280_init(bme280_dev_t *dev, const bme280_hal_t *hal, uint8_t addr);

/**
 * @brief Read the chip id register (0xD0).
 *
 * @param[in]  dev Device with a valid @c hal pointer.
 * @param[out] id  Chip id, ::BME280_CHIP_ID_VALUE for a BME280.
 * @return ::BME280_OK or an error code from bme280_read_regs().
 */
bme280_status_t bme280_read_chip_id(const bme280_dev_t *dev, uint8_t *id);

/**
 * @brief Issue a soft reset (write 0xB6 to 0xE0) and wait for the sensor to restart.
 *
 * @param[in] dev Device with a valid @c hal pointer.
 * @return ::BME280_OK or an error code from bme280_write_reg().
 */
bme280_status_t bme280_soft_reset(const bme280_dev_t *dev);

/**
 * @brief Read both calibration blocks from the sensor and store them in @c dev->calib.
 *
 * @param[in,out] dev Device with a valid @c hal pointer.
 * @return ::BME280_OK or an error code.
 */
bme280_status_t bme280_read_calibration(bme280_dev_t *dev);

/**
 * @brief Decode raw calibration bytes into ::bme280_calib_t (pure function, no I/O).
 *
 * @param[in]  tp  ::BME280_CALIB_TP_LEN bytes read from 0x88..0xA1.
 * @param[in]  h   ::BME280_CALIB_H_LEN bytes read from 0xE1..0xE7.
 * @param[out] out Decoded parameters.
 * @retval BME280_OK         Success.
 * @retval BME280_E_NULL_PTR A pointer is NULL.
 */
bme280_status_t bme280_parse_calibration(const uint8_t tp[BME280_CALIB_TP_LEN],
                                         const uint8_t h[BME280_CALIB_H_LEN], bme280_calib_t *out);

/**
 * @brief Apply oversampling, filter, standby time and mode.
 *
 * Sequence: ctrl_meas with mode = sleep, then ctrl_hum, then config, then
 * ctrl_meas with the requested mode. ctrl_hum only takes effect after a write
 * to ctrl_meas (datasheet section 5.4.3) and config writes may be ignored in
 * normal mode (section 5.4.6), so the sensor is put to sleep first and the
 * mode is written last.
 *
 * @param[in] dev Initialised device.
 * @param[in] cfg Configuration.
 * @retval BME280_OK            Success.
 * @retval BME280_E_NULL_PTR    NULL argument.
 * @retval BME280_E_INVALID_ARG A field is out of range.
 * @retval BME280_E_COMM        I2C error.
 */
bme280_status_t bme280_set_config(const bme280_dev_t *dev, const bme280_config_t *cfg);

/**
 * @brief Change only the power mode (read-modify-write of ctrl_meas).
 *
 * Use ::BME280_MODE_FORCED to trigger a single conversion, then wait
 * bme280_measurement_time_ms() before calling bme280_read_data().
 *
 * @param[in] dev  Initialised device.
 * @param[in] mode New mode.
 * @return ::BME280_OK or an error code.
 */
bme280_status_t bme280_set_mode(const bme280_dev_t *dev, bme280_mode_t mode);

/**
 * @brief Maximum conversion time for a configuration (datasheet appendix B, section 9.1).
 *
 * @param[in] cfg Configuration (NULL returns 0).
 * @return Maximum measurement time in milliseconds, rounded up.
 */
uint32_t bme280_measurement_time_ms(const bme280_config_t *cfg);

/* ------------------------------------------------------------------------- */
/* Measurements                                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Burst-read the 8 data registers (0xF7..0xFE) and unpack the raw ADC values.
 *
 * @param[in]  dev Initialised device.
 * @param[out] raw Raw values.
 * @return ::BME280_OK or an error code.
 */
bme280_status_t bme280_read_uncompensated(const bme280_dev_t *dev, bme280_uncomp_data_t *raw);

/**
 * @brief Read and compensate one measurement.
 *
 * Temperature is always compensated first because it produces @c t_fine,
 * which the pressure and humidity formulas need.
 *
 * @param[in,out] dev  Initialised device (@c t_fine is updated).
 * @param[out]    data Compensated values.
 * @retval BME280_OK         Success.
 * @retval BME280_E_NULL_PTR NULL argument.
 * @retval BME280_E_INVALID_ARG Device not initialised.
 * @retval BME280_E_COMM     I2C error.
 */
bme280_status_t bme280_read_data(bme280_dev_t *dev, bme280_data_t *data);

/* ------------------------------------------------------------------------- */
/* Compensation (pure functions, datasheet section 4.2.3 and 8.2)            */
/* ------------------------------------------------------------------------- */

/**
 * @brief Compensate a raw temperature value (32-bit integer formula).
 *
 * @param[in]  calib  Calibration parameters.
 * @param[in]  adc_T  20-bit raw temperature.
 * @param[out] t_fine Fine temperature for the pressure and humidity formulas (may be NULL).
 * @return Temperature in 0.01 degC. Datasheet example: adc_T = 519888 gives 2508 (25.08 degC).
 */
int32_t bme280_compensate_temperature(const bme280_calib_t *calib, int32_t adc_T, int32_t *t_fine);

/**
 * @brief Compensate a raw pressure value (64-bit integer formula).
 *
 * @param[in] calib  Calibration parameters.
 * @param[in] adc_P  20-bit raw pressure.
 * @param[in] t_fine Value produced by bme280_compensate_temperature().
 * @return Pressure in Pa as Q24.8 (25767236 = 100653.27 Pa, divide by 256). Returns 0 if the
 *         intermediate divisor is 0 (avoids a division by zero).
 */
uint32_t bme280_compensate_pressure(const bme280_calib_t *calib, int32_t adc_P, int32_t t_fine);

/**
 * @brief Compensate a raw humidity value (32-bit integer formula).
 *
 * @param[in] calib  Calibration parameters.
 * @param[in] adc_H  16-bit raw humidity.
 * @param[in] t_fine Value produced by bme280_compensate_temperature().
 * @return Relative humidity as Q22.10 (47445 = 46.333 %RH), clamped to [0, 102400].
 */
uint32_t bme280_compensate_humidity(const bme280_calib_t *calib, int32_t adc_H, int32_t t_fine);

/**
 * @brief Human readable name of a status code (for logs).
 * @param[in] status Status code.
 * @return Static string, never NULL.
 */
const char *bme280_status_str(bme280_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* BME280_H */
