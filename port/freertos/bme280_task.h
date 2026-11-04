/**
 * @file bme280_task.h
 * @brief FreeRTOS task that samples the BME280 periodically and publishes to a queue.
 *
 * The task owns the sensor: no other task may call the driver on the same
 * ::bme280_dev_t while it runs. Consumers receive ::bme280_sample_t items:
 *
 * @code
 * QueueHandle_t q = xQueueCreate(8, sizeof(bme280_sample_t));
 * static bme280_task_config_t cfg = {&dev, BME280_CONFIG_DEFAULT, NULL, 1000U, 0U};
 * cfg.queue = q;
 * bme280_task_start(&cfg, tskIDLE_PRIORITY + 2, BME280_TASK_STACK_WORDS, NULL);
 *
 * bme280_sample_t s;
 * if (xQueueReceive(q, &s, portMAX_DELAY) == pdPASS && s.status == BME280_OK) { ... }
 * @endcode
 *
 * @copyright Copyright (c) 2026 Abdessettar Ben Hassen. MIT License.
 */
#ifndef BME280_TASK_H
#define BME280_TASK_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "bme280.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Default stack depth in words (no printf inside the task). */
#define BME280_TASK_STACK_WORDS 256U

/** @brief One published sample. */
typedef struct {
    TickType_t timestamp;   /**< xTaskGetTickCount() when the sample was read. */
    bme280_status_t status; /**< BME280_OK, or the error of this read (data is then invalid). */
    bme280_data_t data;     /**< Compensated values. */
} bme280_sample_t;

/** @brief Task parameters. Must stay valid while the task runs (use static storage). */
typedef struct {
    bme280_dev_t *dev;      /**< Initialised device (bme280_init() already called). */
    bme280_config_t config; /**< Applied once at task start. */
    QueueHandle_t queue;    /**< Destination queue, item size = sizeof(bme280_sample_t). */
    uint32_t period_ms;     /**< Sampling period, must be >= bme280_measurement_time_ms(). */
    uint32_t dropped;       /**< Incremented by the task when the queue is full. */
} bme280_task_config_t;

/**
 * @brief Validate @p cfg and create the sampling task.
 * @param[in]  cfg         Task parameters (static storage).
 * @param[in]  priority    FreeRTOS priority.
 * @param[in]  stack_words Stack depth in words, see ::BME280_TASK_STACK_WORDS.
 * @param[out] handle      Created task handle (may be NULL).
 * @return pdPASS on success, pdFAIL on invalid parameters or allocation failure.
 */
BaseType_t bme280_task_start(bme280_task_config_t *cfg, UBaseType_t priority,
                             configSTACK_DEPTH_TYPE stack_words, TaskHandle_t *handle);

/**
 * @brief Task entry point (exposed so it can be created statically with xTaskCreateStatic()).
 * @param[in] params Pointer to a ::bme280_task_config_t.
 */
void bme280_task(void *params);

#ifdef __cplusplus
}
#endif

#endif /* BME280_TASK_H */
