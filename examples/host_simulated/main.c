/**
 * @file main.c
 * @brief Run the driver on a PC against the fake I2C bus (no hardware needed).
 *
 * Build and run:
 *   cmake -B build && cmake --build build && ./build/examples/host_simulated/bme280_host_demo
 */
#include <stdio.h>

#include "bme280.h"
#include "fake_i2c.h"

int main(void)
{
    fake_i2c_t bus;
    bme280_hal_t hal;
    bme280_dev_t dev;
    bme280_config_t cfg = BME280_CONFIG_DEFAULT;
    bme280_data_t data;
    bme280_status_t rc;

    fake_i2c_reset(&bus, BME280_I2C_ADDR_PRIMARY);
    fake_i2c_load_bme280_fixture(&bus);
    hal = fake_i2c_hal(&bus);

    printf("BME280 driver %s, host simulation\n", BME280_DRIVER_VERSION);

    rc = bme280_init(&dev, &hal, BME280_I2C_ADDR_PRIMARY);
    if (rc != BME280_OK) {
        printf("init failed: %s\n", bme280_status_str(rc));
        return 1;
    }
    rc = bme280_set_config(&dev, &cfg);
    if (rc != BME280_OK) {
        printf("config failed: %s\n", bme280_status_str(rc));
        return 1;
    }
    rc = bme280_read_data(&dev, &data);
    if (rc != BME280_OK) {
        printf("read failed: %s\n", bme280_status_str(rc));
        return 1;
    }

    {
        const long t = (long)data.temperature;
        const long t_abs = (t < 0) ? -t : t;
        printf("T = %s%ld.%02ld degC\n", (t < 0) ? "-" : "", t_abs / 100, t_abs % 100);
    }
    printf("P = %lu.%02lu hPa\n", (unsigned long)(data.pressure / 25600U),
           (unsigned long)(((data.pressure % 25600U) * 100U) / 25600U));
    printf("H = %lu.%03lu %%RH\n", (unsigned long)(data.humidity >> 10),
           (unsigned long)(((data.humidity & 0x3FFU) * 1000U) >> 10));
    return 0;
}
