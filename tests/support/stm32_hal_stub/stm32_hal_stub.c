/**
 * @file stm32_hal_stub.c
 * @brief Recording fake of the few Cube HAL functions used by port/stm32.
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#include "stm32_hal_stub.h"

#include <string.h>

stm32_stub_t stm32_stub;

void stm32_stub_reset(void)
{
    (void)memset(&stm32_stub, 0, sizeof(stm32_stub));
    stm32_stub.next_status = HAL_OK;
}

static void record(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress,
                   uint16_t MemAddSize, uint16_t Size, uint32_t Timeout)
{
    stm32_stub.last_hi2c = hi2c;
    stm32_stub.last_dev_address = DevAddress;
    stm32_stub.last_mem_address = MemAddress;
    stm32_stub.last_mem_add_size = MemAddSize;
    stm32_stub.last_size = Size;
    stm32_stub.last_timeout = Timeout;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                   uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData,
                                   uint16_t Size, uint32_t Timeout)
{
    stm32_stub.read_calls++;
    record(hi2c, DevAddress, MemAddress, MemAddSize, Size, Timeout);
    if ((stm32_stub.next_status == HAL_OK) && (pData != NULL) &&
        (Size <= sizeof(stm32_stub.read_data))) {
        (void)memcpy(pData, stm32_stub.read_data, Size);
    }
    return stm32_stub.next_status;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                    uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData,
                                    uint16_t Size, uint32_t Timeout)
{
    stm32_stub.write_calls++;
    record(hi2c, DevAddress, MemAddress, MemAddSize, Size, Timeout);
    if ((pData != NULL) && (Size <= sizeof(stm32_stub.written))) {
        (void)memcpy(stm32_stub.written, pData, Size);
    }
    return stm32_stub.next_status;
}

void HAL_Delay(uint32_t Delay)
{
    stm32_stub.delay_calls++;
    stm32_stub.last_delay = Delay;
}
