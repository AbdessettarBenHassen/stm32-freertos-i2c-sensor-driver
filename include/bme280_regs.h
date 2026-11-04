/**
 * @file bme280_regs.h
 * @brief BME280 register map and bit-field definitions.
 *
 * Values come from the Bosch Sensortec BME280 datasheet, section 5.3
 * "Memory map" and section 5.4 "Register description".
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#ifndef BME280_REGS_H
#define BME280_REGS_H

/** @name I2C addresses (section 6.2)
 *  @{ */
#define BME280_I2C_ADDR_PRIMARY   0x76U /**< SDO pin tied to GND. */
#define BME280_I2C_ADDR_SECONDARY 0x77U /**< SDO pin tied to VDDIO. */
/** @} */

/** @name Identification and reset
 *  @{ */
#define BME280_REG_CHIP_ID    0xD0U /**< "id" register, reads ::BME280_CHIP_ID_VALUE. */
#define BME280_CHIP_ID_VALUE  0x60U /**< Expected chip id of a BME280. */
#define BME280_REG_RESET      0xE0U /**< "reset" register. */
#define BME280_SOFT_RESET_CMD 0xB6U /**< Value that triggers a power-on-reset sequence. */
#define BME280_STARTUP_MS     2U    /**< Start-up time after reset (datasheet table 1). */
/** @} */

/** @name Calibration (trimming) parameters, section 4.2.2
 *  @{ */
#define BME280_REG_CALIB_TP_START 0x88U /**< dig_T1 LSB, first byte of the T/P block. */
#define BME280_CALIB_TP_LEN       26U   /**< 0x88..0xA1 inclusive (includes dig_H1 at 0xA1). */
#define BME280_REG_CALIB_H_START  0xE1U /**< dig_H2 LSB, first byte of the humidity block. */
#define BME280_CALIB_H_LEN        7U    /**< 0xE1..0xE7 inclusive. */
/** @} */

/** @name Control, status and data registers
 *  @{ */
#define BME280_REG_CTRL_HUM  0xF2U /**< osrs_h[2:0]. Takes effect after a write to ctrl_meas. */
#define BME280_REG_STATUS    0xF3U /**< measuring[3], im_update[0]. */
#define BME280_REG_CTRL_MEAS 0xF4U /**< osrs_t[7:5], osrs_p[4:2], mode[1:0]. */
#define BME280_REG_CONFIG    0xF5U /**< t_sb[7:5], filter[4:2], spi3w_en[0]. */
#define BME280_REG_DATA      0xF7U /**< press_msb, first byte of the 8-byte burst. */
#define BME280_DATA_LEN      8U    /**< press[3] + temp[3] + hum[2]. */
/** @} */

/** @name Status register bits
 *  @{ */
#define BME280_STATUS_MEASURING 0x08U
#define BME280_STATUS_IM_UPDATE 0x01U
/** @} */

/** Humidity value returned by the sensor when humidity measurement is skipped. */
#define BME280_ADC_H_SKIPPED 0x8000U
/** Temperature/pressure value returned when the measurement is skipped. */
#define BME280_ADC_TP_SKIPPED 0x80000UL

#endif /* BME280_REGS_H */
