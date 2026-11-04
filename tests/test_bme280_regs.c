/**
 * @file test_bme280_regs.c
 * @brief Tests for the low level register helpers bme280_read_regs() / bme280_write_reg().
 */
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
    dev.hal = &hal;
    dev.addr = BME280_I2C_ADDR_PRIMARY;
}

void tearDown(void)
{
}

static void test_read_regs_returns_register_content(void)
{
    uint8_t id = 0U;
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_read_regs(&dev, BME280_REG_CHIP_ID, &id, 1U));
    TEST_ASSERT_EQUAL_HEX8(BME280_CHIP_ID_VALUE, id);
}

static void test_read_regs_forwards_address_register_and_length(void)
{
    uint8_t buf[BME280_DATA_LEN] = {0};
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_read_regs(&dev, BME280_REG_DATA, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT(1U, bus.read_calls);
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_DATA, bus.last_read_reg);
    TEST_ASSERT_EQUAL_UINT(BME280_DATA_LEN, bus.last_read_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(FIXTURE_DATA, buf, BME280_DATA_LEN);
}

static void test_read_regs_rejects_null_pointers(void)
{
    uint8_t b = 0U;
    bme280_dev_t no_hal = {0};
    bme280_hal_t partial = hal;
    bme280_dev_t partial_dev = dev;

    partial.i2c_read = NULL;
    partial_dev.hal = &partial;

    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_read_regs(NULL, 0xD0U, &b, 1U));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_read_regs(&dev, 0xD0U, NULL, 1U));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_read_regs(&no_hal, 0xD0U, &b, 1U));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_read_regs(&partial_dev, 0xD0U, &b, 1U));
    TEST_ASSERT_EQUAL_UINT(0U, bus.read_calls);
}

static void test_read_regs_rejects_zero_length(void)
{
    uint8_t b = 0U;
    TEST_ASSERT_EQUAL_INT(BME280_E_INVALID_ARG, bme280_read_regs(&dev, 0xD0U, &b, 0U));
    TEST_ASSERT_EQUAL_UINT(0U, bus.read_calls);
}

static void test_read_regs_maps_bus_error_to_comm(void)
{
    uint8_t b = 0U;
    bus.fail_reads = 1;
    TEST_ASSERT_EQUAL_INT(BME280_E_COMM, bme280_read_regs(&dev, 0xD0U, &b, 1U));
}

static void test_read_regs_maps_nack_to_comm(void)
{
    uint8_t b = 0U;
    dev.addr = BME280_I2C_ADDR_SECONDARY; /* nobody answers there */
    TEST_ASSERT_EQUAL_INT(BME280_E_COMM, bme280_read_regs(&dev, 0xD0U, &b, 1U));
}

static void test_write_reg_writes_one_byte(void)
{
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_write_reg(&dev, BME280_REG_CTRL_HUM, 0x05U));
    TEST_ASSERT_EQUAL_UINT(1U, bus.write_count);
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_CTRL_HUM, bus.writes[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0x05U, bus.writes[0].value);
    TEST_ASSERT_EQUAL_HEX8(0x05U, bus.regs[BME280_REG_CTRL_HUM]);
}

static void test_write_reg_rejects_null_pointers(void)
{
    bme280_dev_t no_hal = {0};
    bme280_hal_t partial = hal;
    bme280_dev_t partial_dev = dev;

    partial.i2c_write = NULL;
    partial_dev.hal = &partial;

    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_write_reg(NULL, 0xF2U, 1U));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_write_reg(&no_hal, 0xF2U, 1U));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR, bme280_write_reg(&partial_dev, 0xF2U, 1U));
    TEST_ASSERT_EQUAL_UINT(0U, bus.write_calls);
}

static void test_write_reg_maps_bus_error_to_comm(void)
{
    bus.fail_writes = 1;
    TEST_ASSERT_EQUAL_INT(BME280_E_COMM, bme280_write_reg(&dev, 0xF2U, 1U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_read_regs_returns_register_content);
    RUN_TEST(test_read_regs_forwards_address_register_and_length);
    RUN_TEST(test_read_regs_rejects_null_pointers);
    RUN_TEST(test_read_regs_rejects_zero_length);
    RUN_TEST(test_read_regs_maps_bus_error_to_comm);
    RUN_TEST(test_read_regs_maps_nack_to_comm);
    RUN_TEST(test_write_reg_writes_one_byte);
    RUN_TEST(test_write_reg_rejects_null_pointers);
    RUN_TEST(test_write_reg_maps_bus_error_to_comm);
    return UNITY_END();
}
