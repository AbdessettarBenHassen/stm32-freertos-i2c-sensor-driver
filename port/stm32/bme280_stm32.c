/**
 * @file bme280_stm32.c
 * @brief STM32 Cube HAL implementation of ::bme280_hal_t.
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#include "bme280_stm32.h"

#if defined(BME280_STM32_USE_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

void bme280_stm32_hal_bind(bme280_hal_t *hal, bme280_stm32_ctx_t *ctx)
{
    if (hal == NULL) {
        return;
    }
    hal->i2c_read = bme280_stm32_i2c_read;
    hal->i2c_write = bme280_stm32_i2c_write;
    hal->delay_ms = bme280_stm32_delay_ms;
    hal->ctx = ctx;
}

int bme280_stm32_i2c_read(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t *data, size_t len)
{
    const bme280_stm32_ctx_t *c = (const bme280_stm32_ctx_t *)ctx;

    if ((c == NULL) || (c->hi2c == NULL) || (data == NULL) || (len > 0xFFFFU)) {
        return -1;
    }
    /* The Cube HAL takes the 8-bit (left shifted) address. */
    if (HAL_I2C_Mem_Read(c->hi2c, (uint16_t)((uint16_t)dev_addr << 1), (uint16_t)reg,
                         I2C_MEMADD_SIZE_8BIT, data, (uint16_t)len, c->timeout_ms) != HAL_OK) {
        return -1;
    }
    return 0;
}

int bme280_stm32_i2c_write(void *ctx, uint8_t dev_addr, uint8_t reg, const uint8_t *data,
                           size_t len)
{
    const bme280_stm32_ctx_t *c = (const bme280_stm32_ctx_t *)ctx;

    if ((c == NULL) || (c->hi2c == NULL) || (data == NULL) || (len > 0xFFFFU)) {
        return -1;
    }
    /* HAL_I2C_Mem_Write() takes a non-const buffer but does not modify it. */
    if (HAL_I2C_Mem_Write(c->hi2c, (uint16_t)((uint16_t)dev_addr << 1), (uint16_t)reg,
                          I2C_MEMADD_SIZE_8BIT, (uint8_t *)(uintptr_t)data, (uint16_t)len,
                          c->timeout_ms) != HAL_OK) {
        return -1;
    }
    return 0;
}

void bme280_stm32_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
#if defined(BME280_STM32_USE_FREERTOS)
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        TickType_t ticks = pdMS_TO_TICKS(ms);
        /* Round up so a short delay never becomes 0 ticks (no wait at all). */
        if ((ticks == 0U) && (ms != 0U)) {
            ticks = 1U;
        }
        vTaskDelay(ticks);
        return;
    }
#endif
    HAL_Delay(ms);
}
