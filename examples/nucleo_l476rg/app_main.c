/**
 * @file app_main.c
 * @brief NUCLEO-L476RG example: BME280 on I2C1, sampled by a FreeRTOS task, printed on USART2.
 *
 * Drop this file into a STM32CubeIDE / CubeMX project generated with:
 *  - I2C1 enabled (PB8 = SCL / D15, PB9 = SDA / D14), Fast Mode 400 kHz
 *  - USART2 enabled (ST-LINK virtual COM port, 115200 8N1)
 *  - FreeRTOS (CMSIS-RTOS v2 or native) enabled
 * then call app_main() from main() after the MX_..._Init() calls,
 * just before the scheduler is started (osKernelStart() / vTaskStartScheduler()).
 *
 * Add to the project include path: include/, port/stm32/, port/freertos/
 * Add to the sources: src/bme280.c, port/stm32/bme280_stm32.c, port/freertos/bme280_task.c
 * Define BME280_STM32_USE_FREERTOS=1 so the driver delays with vTaskDelay().
 * On a STM32F4 board, also define BME280_STM32_HAL_HEADER="stm32f4xx_hal.h".
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#include <stdio.h>
#include <string.h>

#include "main.h" /* CubeMX generated: hi2c1, huart2 handles and the HAL */

#include "bme280.h"
#include "bme280_stm32.h"
#include "bme280_task.h"

extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;

#define APP_SAMPLE_PERIOD_MS 1000U
#define APP_QUEUE_LENGTH     4U

static bme280_stm32_ctx_t s_i2c_ctx = {.hi2c = &hi2c1, .timeout_ms = 50U};
static bme280_hal_t s_hal;
static bme280_dev_t s_dev;
static bme280_task_config_t s_task_cfg;

static void uart_print(const char *msg)
{
    (void)HAL_UART_Transmit(&huart2, (const uint8_t *)msg, (uint16_t)strlen(msg), 100U);
}

/** Consumer task: waits for samples and prints them on the virtual COM port. */
static void print_task(void *arg)
{
    bme280_sample_t s;
    char line[96];

    (void)arg;
    for (;;) {
        if (xQueueReceive(s_task_cfg.queue, &s, portMAX_DELAY) != pdPASS) {
            continue;
        }
        if (s.status != BME280_OK) {
            (void)snprintf(line, sizeof(line), "[%lu] BME280 error: %s\r\n",
                           (unsigned long)s.timestamp, bme280_status_str(s.status));
        } else {
            /* Integer only printing: no need for printf float support. */
            const int32_t t = s.data.temperature; /* 0.01 degC */
            const uint32_t t_abs = (uint32_t)((t < 0) ? -t : t);
            const uint32_t p = s.data.pressure / 256U;           /* Pa */
            const uint32_t h = (s.data.humidity * 100U) / 1024U; /* 0.01 %RH */
            (void)snprintf(
                line, sizeof(line), "[%lu] T=%s%lu.%02lu C  P=%lu.%02lu hPa  H=%lu.%02lu %%\r\n",
                (unsigned long)s.timestamp, (t < 0) ? "-" : "", (unsigned long)(t_abs / 100U),
                (unsigned long)(t_abs % 100U), (unsigned long)(p / 100U), (unsigned long)(p % 100U),
                (unsigned long)(h / 100U), (unsigned long)(h % 100U));
        }
        uart_print(line);
    }
}

void app_main(void)
{
    bme280_status_t rc;
    const bme280_config_t cfg = BME280_CONFIG_DEFAULT;

    bme280_stm32_hal_bind(&s_hal, &s_i2c_ctx);

    rc = bme280_init(&s_dev, &s_hal, BME280_I2C_ADDR_PRIMARY); /* SDO tied to GND */
    if (rc != BME280_OK) {
        uart_print("BME280 init failed: ");
        uart_print(bme280_status_str(rc));
        uart_print("\r\n");
        return;
    }

    s_task_cfg.dev = &s_dev;
    s_task_cfg.config = cfg;
    s_task_cfg.period_ms = APP_SAMPLE_PERIOD_MS;
    s_task_cfg.queue = xQueueCreate(APP_QUEUE_LENGTH, sizeof(bme280_sample_t));
    configASSERT(s_task_cfg.queue != NULL);

    configASSERT(bme280_task_start(&s_task_cfg, tskIDLE_PRIORITY + 2U, BME280_TASK_STACK_WORDS,
                                   NULL) == pdPASS);
    configASSERT(xTaskCreate(print_task, "print", 384U, NULL, tskIDLE_PRIORITY + 1U, NULL) ==
                 pdPASS);
}
