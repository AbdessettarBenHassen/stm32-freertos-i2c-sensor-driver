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
    /*
     * TODO(Abdessettar): implement with HAL_I2C_Mem_Read().
     *  - Cast ctx to bme280_stm32_ctx_t* and check it (and ctx->hi2c) is not NULL.
     *  - The Cube HAL expects the 8-bit address: pass (uint16_t)(dev_addr << 1).
     *  - Register address size: I2C_MEMADD_SIZE_8BIT.
     *  - len is size_t, the HAL takes uint16_t: reject len > 0xFFFF.
     *  - Return 0 when the HAL returns HAL_OK, -1 otherwise.
     * Tests: tests/test_bme280_stm32_port.c (host, against a Cube HAL stub).
     */
    (void)ctx;
    (void)dev_addr;
    (void)reg;
    (void)data;
    (void)len;
    return -1;
}

int bme280_stm32_i2c_write(void *ctx, uint8_t dev_addr, uint8_t reg, const uint8_t *data,
                           size_t len)
{
    /*
     * TODO(Abdessettar): implement with HAL_I2C_Mem_Write().
     *  - Same address shift and checks as the read callback.
     *  - HAL_I2C_Mem_Write() takes a non-const uint8_t*: cast away const
     *    (the HAL does not modify the buffer).
     * Tests: tests/test_bme280_stm32_port.c
     */
    (void)ctx;
    (void)dev_addr;
    (void)reg;
    (void)data;
    (void)len;
    return -1;
}

void bme280_stm32_delay_ms(void *ctx, uint32_t ms)
{
    /*
     * TODO(Abdessettar): implement the delay.
     *  - Bare metal: HAL_Delay(ms).
     *  - With BME280_STM32_USE_FREERTOS defined and the scheduler running
     *    (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING): vTaskDelay(pdMS_TO_TICKS(ms)),
     *    so other tasks can run while we wait. Make sure a 1 ms request never becomes 0 ticks.
     * Tests: tests/test_bme280_stm32_port.c (bare metal branch).
     */
    (void)ctx;
    (void)ms;
}
