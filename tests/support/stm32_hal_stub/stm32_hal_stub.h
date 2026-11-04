/**
 * @file stm32_hal_stub.h
 * @brief Minimal stand-in for the STM32 Cube HAL, used to unit test port/stm32 on a PC.
 *
 * Only the symbols used by the BME280 STM32 binding are declared, with the
 * same signatures as the real Cube HAL. Every call is recorded in
 * ::stm32_stub so tests can check the arguments (e.g. the 8-bit address shift).
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#ifndef STM32_HAL_STUB_H
#define STM32_HAL_STUB_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    HAL_OK = 0x00U,
    HAL_ERROR = 0x01U,
    HAL_BUSY = 0x02U,
    HAL_TIMEOUT = 0x03U,
} HAL_StatusTypeDef;

typedef struct {
    int instance; /**< Placeholder, the real handle holds the peripheral state. */
} I2C_HandleTypeDef;

#define I2C_MEMADD_SIZE_8BIT  (0x00000001U)
#define I2C_MEMADD_SIZE_16BIT (0x00000002U)

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                   uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData,
                                   uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                    uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData,
                                    uint16_t Size, uint32_t Timeout);
void HAL_Delay(uint32_t Delay);

/** Recorded calls and programmable results. */
typedef struct {
    HAL_StatusTypeDef next_status; /**< Returned by the next Mem_Read / Mem_Write. */
    unsigned read_calls;
    unsigned write_calls;
    unsigned delay_calls;
    I2C_HandleTypeDef *last_hi2c;
    uint16_t last_dev_address;
    uint16_t last_mem_address;
    uint16_t last_mem_add_size;
    uint16_t last_size;
    uint32_t last_timeout;
    uint32_t last_delay;
    uint8_t read_data[32]; /**< Copied into pData by Mem_Read. */
    uint8_t written[32];   /**< Copy of pData given to Mem_Write. */
} stm32_stub_t;

extern stm32_stub_t stm32_stub;

void stm32_stub_reset(void);

#endif /* STM32_HAL_STUB_H */
