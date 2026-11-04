/**
 * @file test_bme280_stm32_port.c
 * @brief Host tests for the STM32 binding (port/stm32) against a recording Cube HAL stub.
 */
#include "bme280.h"
#include "bme280_stm32.h"
#include "unity.h"

static I2C_HandleTypeDef hi2c1;
static bme280_stm32_ctx_t ctx;
static bme280_hal_t hal;

void setUp(void)
{
    stm32_stub_reset();
    ctx.hi2c = &hi2c1;
    ctx.timeout_ms = 25U;
    bme280_stm32_hal_bind(&hal, &ctx);
}

void tearDown(void)
{
}

static void test_bind_fills_all_callbacks(void)
{
    TEST_ASSERT_NOT_NULL(hal.i2c_read);
    TEST_ASSERT_NOT_NULL(hal.i2c_write);
    TEST_ASSERT_NOT_NULL(hal.delay_ms);
    TEST_ASSERT_EQUAL_PTR(&ctx, hal.ctx);
}

static void test_read_uses_shifted_address_and_8bit_register(void)
{
    uint8_t buf[2] = {0};
    stm32_stub.read_data[0] = 0x60U;
    stm32_stub.read_data[1] = 0x42U;

    TEST_ASSERT_EQUAL_INT(0, hal.i2c_read(hal.ctx, BME280_I2C_ADDR_PRIMARY, 0xD0U, buf, 2U));
    TEST_ASSERT_EQUAL_UINT(1U, stm32_stub.read_calls);
    TEST_ASSERT_EQUAL_PTR(&hi2c1, stm32_stub.last_hi2c);
    TEST_ASSERT_EQUAL_HEX16(0xECU, stm32_stub.last_dev_address); /* 0x76 << 1 */
    TEST_ASSERT_EQUAL_HEX16(0xD0U, stm32_stub.last_mem_address);
    TEST_ASSERT_EQUAL_UINT16(I2C_MEMADD_SIZE_8BIT, stm32_stub.last_mem_add_size);
    TEST_ASSERT_EQUAL_UINT16(2U, stm32_stub.last_size);
    TEST_ASSERT_EQUAL_UINT32(25U, stm32_stub.last_timeout);
    TEST_ASSERT_EQUAL_HEX8(0x60U, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x42U, buf[1]);
}

static void test_read_reports_hal_error(void)
{
    uint8_t b = 0U;
    stm32_stub.next_status = HAL_TIMEOUT;
    TEST_ASSERT_NOT_EQUAL(0, hal.i2c_read(hal.ctx, BME280_I2C_ADDR_PRIMARY, 0xD0U, &b, 1U));
}

static void test_read_rejects_null_context(void)
{
    uint8_t b = 0U;
    TEST_ASSERT_NOT_EQUAL(0, bme280_stm32_i2c_read(NULL, BME280_I2C_ADDR_PRIMARY, 0xD0U, &b, 1U));
    TEST_ASSERT_EQUAL_UINT(0U, stm32_stub.read_calls);
}

static void test_write_uses_shifted_address(void)
{
    const uint8_t v = 0xB6U;
    TEST_ASSERT_EQUAL_INT(0, hal.i2c_write(hal.ctx, BME280_I2C_ADDR_SECONDARY, 0xE0U, &v, 1U));
    TEST_ASSERT_EQUAL_UINT(1U, stm32_stub.write_calls);
    TEST_ASSERT_EQUAL_HEX16(0xEEU, stm32_stub.last_dev_address); /* 0x77 << 1 */
    TEST_ASSERT_EQUAL_HEX16(0xE0U, stm32_stub.last_mem_address);
    TEST_ASSERT_EQUAL_UINT16(1U, stm32_stub.last_size);
    TEST_ASSERT_EQUAL_HEX8(0xB6U, stm32_stub.written[0]);
}

static void test_write_reports_hal_error(void)
{
    const uint8_t v = 0U;
    stm32_stub.next_status = HAL_ERROR;
    TEST_ASSERT_NOT_EQUAL(0, hal.i2c_write(hal.ctx, BME280_I2C_ADDR_PRIMARY, 0xF2U, &v, 1U));
}

static void test_delay_forwards_to_hal_delay(void)
{
    hal.delay_ms(hal.ctx, 2U);
    TEST_ASSERT_EQUAL_UINT(1U, stm32_stub.delay_calls);
    TEST_ASSERT_EQUAL_UINT32(2U, stm32_stub.last_delay);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bind_fills_all_callbacks);
    RUN_TEST(test_read_uses_shifted_address_and_8bit_register);
    RUN_TEST(test_read_reports_hal_error);
    RUN_TEST(test_read_rejects_null_context);
    RUN_TEST(test_write_uses_shifted_address);
    RUN_TEST(test_write_reports_hal_error);
    RUN_TEST(test_delay_forwards_to_hal_delay);
    return UNITY_END();
}
