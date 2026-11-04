/**
 * @file bme280_task.c
 * @brief FreeRTOS sampling task for the BME280.
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#include "bme280_task.h"

#include <string.h>

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
    const bool forced = (cfg->config.mode == BME280_MODE_FORCED);
    bme280_status_t cfg_status = bme280_set_config(cfg->dev, &cfg->config);
    TickType_t last_wake = xTaskGetTickCount();
    bme280_sample_t sample;

    for (;;) {
        /* Fixed rate: the period is measured from the last wake, so it does not drift. */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(cfg->period_ms));

        sample.status = cfg_status;
        if ((sample.status == BME280_OK) && forced) {
            sample.status = bme280_set_mode(cfg->dev, BME280_MODE_FORCED);
            if (sample.status == BME280_OK) {
                TickType_t wait = pdMS_TO_TICKS(bme280_measurement_time_ms(&cfg->config));
                vTaskDelay((wait == 0U) ? 1U : wait);
            }
        }
        if (sample.status == BME280_OK) {
            sample.status = bme280_read_data(cfg->dev, &sample.data);
        } else {
            (void)memset(&sample.data, 0, sizeof(sample.data));
        }
        sample.timestamp = xTaskGetTickCount();

        /* Never block the producer: drop the sample if the consumer is late. */
        if (xQueueSend(cfg->queue, &sample, 0) != pdPASS) {
            cfg->dropped++;
        }
    }
}
