/**
 * @file fake_i2c.h
 * @brief In-memory fake of a BME280 on an I2C bus, for host unit tests.
 *
 * The fake models the sensor as a 256-byte register map. Tests preload it
 * with recorded register values (chip id, calibration, raw data), then hand
 * fake_i2c_hal() to the driver. Every write is logged so tests can check the
 * exact register sequence, and bus errors can be injected.
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#ifndef FAKE_I2C_H
#define FAKE_I2C_H

#include <stddef.h>
#include <stdint.h>

#include "bme280_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FAKE_I2C_MAX_WRITES 64U

/** One logged register write. */
typedef struct {
    uint8_t reg;
    uint8_t value;
} fake_i2c_write_t;

/** State of the fake bus. */
typedef struct {
    uint8_t dev_addr;  /**< Address the fake device answers to (others NACK). */
    uint8_t regs[256]; /**< Register map. */
    int fail_reads;    /**< Non-zero: every read returns an error. */
    int fail_writes;   /**< Non-zero: every write returns an error. */
    int fail_read_at;  /**< 1-based index of the read call that fails, 0 = never. */
    unsigned read_calls;
    unsigned write_calls;
    unsigned delay_calls;
    uint32_t total_delay_ms;
    uint8_t last_read_reg;
    size_t last_read_len;
    fake_i2c_write_t writes[FAKE_I2C_MAX_WRITES];
    size_t write_count;
} fake_i2c_t;

/** Reset the fake: zeroed registers, no failure, answers on @p dev_addr. */
void fake_i2c_reset(fake_i2c_t *bus, uint8_t dev_addr);

/** Copy @p len bytes into the register map starting at @p reg. */
void fake_i2c_load(fake_i2c_t *bus, uint8_t reg, const uint8_t *data, size_t len);

/** Build a HAL bound to @p bus. */
bme280_hal_t fake_i2c_hal(fake_i2c_t *bus);

/** HAL callbacks, exposed so tests can build partial HALs. */
int fake_i2c_read(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t *data, size_t len);
int fake_i2c_write(void *ctx, uint8_t dev_addr, uint8_t reg, const uint8_t *data, size_t len);
void fake_i2c_delay_ms(void *ctx, uint32_t ms);

/* ------------------------------------------------------------------------- */
/* Recorded fixture                                                          */
/* ------------------------------------------------------------------------- */

/**
 * Load a complete "known good" BME280 image into the fake:
 * chip id 0x60, calibration from the Bosch datasheet worked example
 * (dig_T1 = 27504, ..., dig_P9 = 6000) plus a realistic humidity set,
 * and raw data adc_T = 519888, adc_P = 415148, adc_H = 30000.
 */
void fake_i2c_load_bme280_fixture(fake_i2c_t *bus);

/** Raw calibration bytes used by the fixture (0x88..0xA1). */
extern const uint8_t FIXTURE_CALIB_TP[26];
/** Raw calibration bytes used by the fixture (0xE1..0xE7). */
extern const uint8_t FIXTURE_CALIB_H[7];
/** Raw data bytes used by the fixture (0xF7..0xFE). */
extern const uint8_t FIXTURE_DATA[8];

#ifdef __cplusplus
}
#endif

#endif /* FAKE_I2C_H */
