/**
 * @file test_bme280_data.c
 * @brief Tests for configuration, raw data unpacking and end-to-end measurement.
 */
#include <string.h>

#include "bme280.h"
#include "fake_i2c.h"
#include "unity.h"

static fake_i2c_t bus;
static bme280_hal_t hal;
static bme280_dev_t dev;

void setUp(void)
{
    fake_i2c_reset(&bus, BME280_I2C_ADDR_PRIMARY);
    fake_i2c_load_bme280_fixture(&bus);
    hal = fake_i2c_hal(&bus);
    (void)memset(&dev, 0, sizeof(dev));
    dev.hal = &hal;
    dev.addr = BME280_I2C_ADDR_PRIMARY;
}

void tearDown(void)
{
}

/* ---------------------------- configuration ------------------------------ */

static void test_set_config_writes_registers_in_order(void)
{
    const bme280_config_t cfg = {BME280_OSR_X2,    BME280_OSR_X16,        BME280_OSR_X1,
                                 BME280_FILTER_16, BME280_STANDBY_125_MS, BME280_MODE_NORMAL};

    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_set_config(&dev, &cfg));
    TEST_ASSERT_EQUAL_UINT(4U, bus.write_count);
    /* 1. sleep, keep oversampling */
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_CTRL_MEAS, bus.writes[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0x54U, bus.writes[0].value);
    /* 2. ctrl_hum before the final ctrl_meas */
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_CTRL_HUM, bus.writes[1].reg);
    TEST_ASSERT_EQUAL_HEX8(0x01U, bus.writes[1].value);
    /* 3. config: t_sb = 010, filter = 100 */
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_CONFIG, bus.writes[2].reg);
    TEST_ASSERT_EQUAL_HEX8(0x50U, bus.writes[2].value);
    /* 4. ctrl_meas: osrs_t = 010, osrs_p = 101, mode = 11 */
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_CTRL_MEAS, bus.writes[3].reg);
    TEST_ASSERT_EQUAL_HEX8(0x57U, bus.writes[3].value);
}

static void test_set_config_rejects_invalid_values(void)
{
    bme280_config_t cfg = BME280_CONFIG_DEFAULT;
    cfg.mode = (bme280_mode_t)2; /* reserved encoding */
    TEST_ASSERT_EQUAL_INT(BME280_E_INVALID_ARG, bme280_set_config(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_set_config(&dev, NULL));
    TEST_ASSERT_EQUAL_UINT(0U, bus.write_calls);
}

static void test_set_mode_forced_keeps_oversampling(void)
{
    bus.regs[BME280_REG_CTRL_MEAS] = 0x54U;
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_set_mode(&dev, BME280_MODE_FORCED));
    TEST_ASSERT_EQUAL_HEX8(0x55U, bus.regs[BME280_REG_CTRL_MEAS]);
}

static void test_measurement_time(void)
{
    const bme280_config_t def = BME280_CONFIG_DEFAULT;
    bme280_config_t cfg = BME280_CONFIG_DEFAULT;

    TEST_ASSERT_EQUAL_UINT32(10U, bme280_measurement_time_ms(&def)); /* 9.3 ms */
    cfg.osr_t = BME280_OSR_X16;
    cfg.osr_p = BME280_OSR_X16;
    cfg.osr_h = BME280_OSR_X16;
    TEST_ASSERT_EQUAL_UINT32(113U, bme280_measurement_time_ms(&cfg)); /* 112.8 ms */
    cfg.osr_t = BME280_OSR_X1;
    cfg.osr_p = BME280_OSR_SKIP;
    cfg.osr_h = BME280_OSR_SKIP;
    TEST_ASSERT_EQUAL_UINT32(4U, bme280_measurement_time_ms(&cfg)); /* 3.55 ms */
    TEST_ASSERT_EQUAL_UINT32(0U, bme280_measurement_time_ms(NULL));
}

/* ------------------------------ raw data --------------------------------- */

static void test_read_uncompensated_unpacks_20_and_16_bit_values(void)
{
    bme280_uncomp_data_t raw;
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_read_uncompensated(&dev, &raw));
    TEST_ASSERT_EQUAL_UINT32(415148U, raw.pressure);
    TEST_ASSERT_EQUAL_UINT32(519888U, raw.temperature);
    TEST_ASSERT_EQUAL_UINT32(30000U, raw.humidity);
    TEST_ASSERT_EQUAL_UINT(1U, bus.read_calls); /* single burst read for consistency */
}

/* ------------------------------ end to end ------------------------------- */

static void test_read_data_end_to_end(void)
{
    bme280_data_t data;
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_init(&dev, &hal, BME280_I2C_ADDR_PRIMARY));
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_read_data(&dev, &data));
    TEST_ASSERT_EQUAL_INT32(2508, data.temperature);
    TEST_ASSERT_UINT32_WITHIN(16U, 25767236U, data.pressure);
    TEST_ASSERT_EQUAL_UINT32(56317U, data.humidity);
    TEST_ASSERT_EQUAL_INT32(128422, dev.t_fine);
}

static void test_read_data_requires_init(void)
{
    bme280_data_t data;
    TEST_ASSERT_EQUAL_INT(BME280_E_INVALID_ARG, bme280_read_data(&dev, &data));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_read_data(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_read_data(NULL, &data));
}

static void test_read_data_propagates_bus_error(void)
{
    bme280_data_t data;
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_init(&dev, &hal, BME280_I2C_ADDR_PRIMARY));
    bus.fail_reads = 1;
    TEST_ASSERT_EQUAL_INT(BME280_E_COMM, bme280_read_data(&dev, &data));
}

static void test_status_str_never_null(void)
{
    TEST_ASSERT_EQUAL_STRING("OK", bme280_status_str(BME280_OK));
    TEST_ASSERT_NOT_NULL(bme280_status_str((bme280_status_t)-99));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_set_config_writes_registers_in_order);
    RUN_TEST(test_set_config_rejects_invalid_values);
    RUN_TEST(test_set_mode_forced_keeps_oversampling);
    RUN_TEST(test_measurement_time);
    RUN_TEST(test_read_uncompensated_unpacks_20_and_16_bit_values);
    RUN_TEST(test_read_data_end_to_end);
    RUN_TEST(test_read_data_requires_init);
    RUN_TEST(test_read_data_propagates_bus_error);
    RUN_TEST(test_status_str_never_null);
    return UNITY_END();
}
