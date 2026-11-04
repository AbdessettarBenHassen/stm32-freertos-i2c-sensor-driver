/**
 * @file bme280_task.c
 * @brief FreeRTOS sampling task for the BME280.
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#include "bme280_task.h"

BaseType_t bme280_task_start(bme280_task_config_t *cfg, UBaseType_t priority,
                             configSTACK_DEPTH_TYPE stack_words, TaskHandle_t *handle)
{
    if ((cfg == NULL) || (cfg->dev == NULL) || (cfg->queue == NULL) || (cfg->period_ms == 0U)) {
        return pdFAIL;
    }
    if (cfg->period_ms < bme280_measurement_time_ms(&cfg->config)) {
        return pdFAIL;
    }
    cfg->dropped = 0U;
    return xTaskCreate(bme280_task, "bme280", stack_words, cfg, priority, handle);
}

void bme280_task(void *params)
{
    bme280_task_config_t *cfg = (bme280_task_config_t *)params;

    /*
     * TODO(Abdessettar): implement the task body.
     *  1. Apply cfg->config once with bme280_set_config().
     *  2. Initialise a TickType_t "last wake" with xTaskGetTickCount().
     *  3. Forever:
     *     a. vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(cfg->period_ms))
     *        (fixed rate, no drift, unlike vTaskDelay()).
     *     b. If the config uses BME280_MODE_FORCED: bme280_set_mode(FORCED), then
     *        vTaskDelay() for bme280_measurement_time_ms(&cfg->config).
     *     c. Fill a bme280_sample_t: timestamp, status = bme280_read_data(...), data.
     *     d. xQueueSend(cfg->queue, &sample, 0): never block the producer.
     *        If it fails, increment cfg->dropped.
     *  The task must never return: a FreeRTOS task function that returns is a bug.
     * Tests: tests/freertos_sim/test_bme280_task_sim.c (cmake -DBME280_FREERTOS_SIM=ON).
     */
    (void)cfg;
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}
