/**
 * @file test_bme280_init.c
 * @brief Tests for bme280_init(): chip id check, soft reset, calibration loading, errors.
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
}

void tearDown(void)
{
}

static void test_init_succeeds_with_valid_chip(void)
{
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_init(&dev, &hal, BME280_I2C_ADDR_PRIMARY));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_EQUAL_HEX8(BME280_CHIP_ID_VALUE, dev.chip_id);
    TEST_ASSERT_EQUAL_HEX8(BME280_I2C_ADDR_PRIMARY, dev.addr);
}

static void test_read_chip_id(void)
{
    uint8_t id = 0U;
    dev.hal = &hal;
    dev.addr = BME280_I2C_ADDR_PRIMARY;
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_read_chip_id(&dev, &id));
    TEST_ASSERT_EQUAL_HEX8(0x60U, id);
}

static void test_init_works_on_secondary_address(void)
{
    fake_i2c_reset(&bus, BME280_I2C_ADDR_SECONDARY);
    fake_i2c_load_bme280_fixture(&bus);
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_init(&dev, &hal, BME280_I2C_ADDR_SECONDARY));
}

static void test_init_issues_soft_reset_and_waits(void)
{
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_init(&dev, &hal, BME280_I2C_ADDR_PRIMARY));
    TEST_ASSERT_EQUAL_UINT(1U, bus.write_count);
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_RESET, bus.writes[0].reg);
    TEST_ASSERT_EQUAL_HEX8(BME280_SOFT_RESET_CMD, bus.writes[0].value);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(BME280_STARTUP_MS, bus.total_delay_ms);
}

static void test_init_loads_calibration(void)
{
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_init(&dev, &hal, BME280_I2C_ADDR_PRIMARY));
    TEST_ASSERT_EQUAL_UINT16(27504U, dev.calib.dig_T1);
    TEST_ASSERT_EQUAL_INT16(6000, dev.calib.dig_P9);
    TEST_ASSERT_EQUAL_INT8(30, dev.calib.dig_H6);
}

static void test_init_rejects_wrong_chip_id(void)
{
    bus.regs[BME280_REG_CHIP_ID] = 0x58U; /* BMP280: no humidity sensor */
    TEST_ASSERT_EQUAL_INT(BME280_E_CHIP_ID, bme280_init(&dev, &hal, BME280_I2C_ADDR_PRIMARY));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_EQUAL_UINT(0U, bus.write_count); /* must not reset an unknown device */
}

static void test_init_rejects_null_arguments(void)
{
    bme280_hal_t partial = hal;
    partial.delay_ms = NULL;

    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_init(NULL, &hal, BME280_I2C_ADDR_PRIMARY));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_init(&dev, NULL, BME280_I2C_ADDR_PRIMARY));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_init(&dev, &partial, BME280_I2C_ADDR_PRIMARY));
}

static void test_init_rejects_invalid_address(void)
{
    TEST_ASSERT_EQUAL_INT(BME280_E_INVALID_ARG, bme280_init(&dev, &hal, 0x42U));
    TEST_ASSERT_EQUAL_UINT(0U, bus.read_calls);
}

static void test_init_reports_missing_device(void)
{
    /* Device answers on 0x76, driver probes 0x77 -> NACK. */
    TEST_ASSERT_EQUAL_INT(BME280_E_COMM, bme280_init(&dev, &hal, BME280_I2C_ADDR_SECONDARY));
    TEST_ASSERT_FALSE(dev.initialized);
}

static void test_init_reports_error_while_reading_calibration(void)
{
    bus.fail_read_at = 2; /* 1st read = chip id, 2nd read = calibration */
    TEST_ASSERT_EQUAL_INT(BME280_E_COMM, bme280_init(&dev, &hal, BME280_I2C_ADDR_PRIMARY));
    TEST_ASSERT_FALSE(dev.initialized);
}

static void test_init_reports_error_on_reset_write(void)
{
    bus.fail_writes = 1;
    TEST_ASSERT_EQUAL_INT(BME280_E_COMM, bme280_init(&dev, &hal, BME280_I2C_ADDR_PRIMARY));
    TEST_ASSERT_FALSE(dev.initialized);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_succeeds_with_valid_chip);
    RUN_TEST(test_read_chip_id);
    RUN_TEST(test_init_works_on_secondary_address);
    RUN_TEST(test_init_issues_soft_reset_and_waits);
    RUN_TEST(test_init_loads_calibration);
    RUN_TEST(test_init_rejects_wrong_chip_id);
    RUN_TEST(test_init_rejects_null_arguments);
    RUN_TEST(test_init_rejects_invalid_address);
    RUN_TEST(test_init_reports_missing_device);
    RUN_TEST(test_init_reports_error_while_reading_calibration);
    RUN_TEST(test_init_reports_error_on_reset_write);
    return UNITY_END();
}
