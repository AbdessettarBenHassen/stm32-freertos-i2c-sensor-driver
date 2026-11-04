/**
 * @file bme280_hal.h
 * @brief Hardware abstraction layer (HAL) interface used by the BME280 driver.
 *
 * The driver never talks to a microcontroller peripheral directly. Instead, the
 * application provides a ::bme280_hal_t instance filled with function pointers
 * for I2C register access and millisecond delays. This keeps the driver core
 * portable: the same code runs on an STM32 (see `port/stm32/`), on any other
 * MCU, or on a PC against a fake I2C bus for unit testing (see `tests/support/`).
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#ifndef BME280_HAL_H
#define BME280_HAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read @p len consecutive bytes starting at register @p reg.
 *
 * The implementation must perform a standard I2C "write register address,
 * repeated start, read N bytes" transaction. The BME280 auto-increments the
 * register address during burst reads.
 *
 * @param[in]  ctx      User context pointer (::bme280_hal_t::ctx), e.g. an I2C handle.
 * @param[in]  dev_addr 7-bit I2C device address (0x76 or 0x77), NOT shifted.
 * @param[in]  reg      First register address to read.
 * @param[out] data     Destination buffer, at least @p len bytes.
 * @param[in]  len      Number of bytes to read.
 * @return 0 on success, any non-zero value on bus error (NACK, timeout, ...).
 */
typedef int (*bme280_i2c_read_fn)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t *data,
                                  size_t len);

/**
 * @brief Write @p len bytes starting at register @p reg.
 *
 * @param[in] ctx      User context pointer (::bme280_hal_t::ctx).
 * @param[in] dev_addr 7-bit I2C device address (0x76 or 0x77), NOT shifted.
 * @param[in] reg      First register address to write.
 * @param[in] data     Bytes to write.
 * @param[in] len      Number of bytes to write.
 * @return 0 on success, any non-zero value on bus error.
 *
 * @note The BME280 does NOT auto-increment on writes: to write several
 *       registers the driver issues one call per register.
 */
typedef int (*bme280_i2c_write_fn)(void *ctx, uint8_t dev_addr, uint8_t reg, const uint8_t *data,
                                   size_t len);

/**
 * @brief Block the caller for at least @p ms milliseconds.
 *
 * Bare-metal ports typically use `HAL_Delay()`, RTOS ports `vTaskDelay()`.
 *
 * @param[in] ctx User context pointer (::bme280_hal_t::ctx).
 * @param[in] ms  Minimum delay in milliseconds.
 */
typedef void (*bme280_delay_ms_fn)(void *ctx, uint32_t ms);

/**
 * @brief Platform binding passed to the driver.
 *
 * All function pointers are mandatory. The structure must outlive the
 * ::bme280_dev_t that references it.
 */
typedef struct {
    bme280_i2c_read_fn i2c_read;   /**< Burst register read. */
    bme280_i2c_write_fn i2c_write; /**< Register write. */
    bme280_delay_ms_fn delay_ms;   /**< Millisecond delay. */
    void *ctx;                     /**< Opaque user context forwarded to every callback. */
} bme280_hal_t;

#ifdef __cplusplus
}
#endif

#endif /* BME280_HAL_H */
