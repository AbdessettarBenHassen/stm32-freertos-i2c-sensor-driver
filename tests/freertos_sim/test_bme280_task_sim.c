/**
 * @file test_bme280_task_sim.c
 * @brief Integration test: run bme280_task() on the FreeRTOS POSIX simulator with the fake bus.
 *
 * A consumer task receives samples from the queue and checks them, then stops
 * the scheduler. Exit code 0 means pass.
 */
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "bme280.h"
#include "bme280_task.h"
#include "fake_i2c.h"

#define SAMPLES_TO_CHECK 3U
#define PERIOD_MS        20U

static fake_i2c_t bus;
static bme280_hal_t hal;
static bme280_dev_t dev;
static bme280_task_config_t task_cfg;
static volatile int result = 1;

void vAssertCalled(const char *file, unsigned long line)
{
    (void)fprintf(stderr, "configASSERT failed at %s:%lu\n", file, line);
    abort();
}

static int check_sample(const bme280_sample_t *s, unsigned idx)
{
    if (s->status != BME280_OK) {
        (void)printf("sample %u: status %s\n", idx, bme280_status_str(s->status));
        return 0;
    }
    if ((s->data.temperature != 2508) || (s->data.humidity != 56317U) ||
        (s->data.pressure / 256U != 100653U)) {
        (void)printf("sample %u: unexpected values T=%ld P=%lu H=%lu\n", idx,
                     (long)s->data.temperature, (unsigned long)s->data.pressure,
                     (unsigned long)s->data.humidity);
        return 0;
    }
    return 1;
}

static void consumer_task(void *arg)
{
    bme280_sample_t s;
    TickType_t prev = 0;
    unsigned i;
    int ok = 1;

    (void)arg;
    for (i = 0U; (i < SAMPLES_TO_CHECK) && ok; i++) {
        if (xQueueReceive(task_cfg.queue, &s, pdMS_TO_TICKS(10U * PERIOD_MS)) != pdPASS) {
            (void)printf("FAIL: no sample %u received within %u ms\n", i, 10U * PERIOD_MS);
            ok = 0;
            break;
        }
        ok = check_sample(&s, i);
        if (ok && (i > 0U) && ((TickType_t)(s.timestamp - prev) < pdMS_TO_TICKS(PERIOD_MS / 2U))) {
            (void)printf("FAIL: samples %u and %u are too close (period not respected)\n", i - 1U,
                         i);
            ok = 0;
        }
        prev = s.timestamp;
    }
    if (ok) {
        (void)printf("PASS: %u samples received and checked\n", SAMPLES_TO_CHECK);
    }
    result = ok ? 0 : 1;
    vTaskEndScheduler();
    vTaskDelete(NULL);
}

int main(void)
{
    bme280_status_t rc;
    const bme280_config_t cfg = BME280_CONFIG_DEFAULT;

    fake_i2c_reset(&bus, BME280_I2C_ADDR_PRIMARY);
    fake_i2c_load_bme280_fixture(&bus);
    hal = fake_i2c_hal(&bus);

    rc = bme280_init(&dev, &hal, BME280_I2C_ADDR_PRIMARY);
    if (rc != BME280_OK) {
        (void)printf("FAIL: bme280_init: %s (implement the driver core first)\n",
                     bme280_status_str(rc));
        return 1;
    }

    task_cfg.dev = &dev;
    task_cfg.config = cfg;
    task_cfg.period_ms = PERIOD_MS;
    task_cfg.queue = xQueueCreate(4, sizeof(bme280_sample_t));
    if ((task_cfg.queue == NULL) ||
        (bme280_task_start(&task_cfg, tskIDLE_PRIORITY + 2U, BME280_TASK_STACK_WORDS, NULL) !=
         pdPASS) ||
        (xTaskCreate(consumer_task, "consumer", configMINIMAL_STACK_SIZE * 4U, NULL,
                     tskIDLE_PRIORITY + 1U, NULL) != pdPASS)) {
        (void)printf("FAIL: could not create the queue or the tasks\n");
        return 1;
    }

    vTaskStartScheduler();
    return result;
}
