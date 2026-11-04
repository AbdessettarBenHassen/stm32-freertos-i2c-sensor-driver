/**
 * @file test_bme280_calib.c
 * @brief Tests for calibration parsing (bme280_parse_calibration) and reading
 * (bme280_read_calibration).
 */
#include <string.h>

#include "bme280.h"
#include "fake_i2c.h"
#include "unity.h"

static bme280_calib_t calib;

void setUp(void)
{
    (void)memset(&calib, 0xA5, sizeof(calib)); /* garbage, must be overwritten */
}

void tearDown(void)
{
}

static void assert_fixture_calibration(const bme280_calib_t *c)
{
    TEST_ASSERT_EQUAL_UINT16(27504U, c->dig_T1);
    TEST_ASSERT_EQUAL_INT16(26435, c->dig_T2);
    TEST_ASSERT_EQUAL_INT16(-1000, c->dig_T3);
    TEST_ASSERT_EQUAL_UINT16(36477U, c->dig_P1);
    TEST_ASSERT_EQUAL_INT16(-10685, c->dig_P2);
    TEST_ASSERT_EQUAL_INT16(3024, c->dig_P3);
    TEST_ASSERT_EQUAL_INT16(2855, c->dig_P4);
    TEST_ASSERT_EQUAL_INT16(140, c->dig_P5);
    TEST_ASSERT_EQUAL_INT16(-7, c->dig_P6);
    TEST_ASSERT_EQUAL_INT16(15500, c->dig_P7);
    TEST_ASSERT_EQUAL_INT16(-14600, c->dig_P8);
    TEST_ASSERT_EQUAL_INT16(6000, c->dig_P9);
    TEST_ASSERT_EQUAL_UINT8(75U, c->dig_H1);
    TEST_ASSERT_EQUAL_INT16(362, c->dig_H2);
    TEST_ASSERT_EQUAL_UINT8(0U, c->dig_H3);
    TEST_ASSERT_EQUAL_INT16(313, c->dig_H4);
    TEST_ASSERT_EQUAL_INT16(50, c->dig_H5);
    TEST_ASSERT_EQUAL_INT8(30, c->dig_H6);
}

static void test_parse_temperature_and_pressure_words(void)
{
    TEST_ASSERT_EQUAL_INT(BME280_OK,
                          bme280_parse_calibration(FIXTURE_CALIB_TP, FIXTURE_CALIB_H, &calib));
    assert_fixture_calibration(&calib);
}

static void test_parse_unsigned_words_above_32767(void)
{
    uint8_t tp[BME280_CALIB_TP_LEN] = {0};
    uint8_t h[BME280_CALIB_H_LEN] = {0};
    tp[0] = 0xFFU; /* dig_T1 = 0xFFFF must stay 65535, not -1 */
    tp[1] = 0xFFU;
    tp[6] = 0x00U; /* dig_P1 = 0x8000 = 32768 */
    tp[7] = 0x80U;
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_parse_calibration(tp, h, &calib));
    TEST_ASSERT_EQUAL_UINT16(65535U, calib.dig_T1);
    TEST_ASSERT_EQUAL_UINT16(32768U, calib.dig_P1);
}

static void test_parse_humidity_nibbles_negative(void)
{
    uint8_t tp[BME280_CALIB_TP_LEN] = {0};
    uint8_t h[BME280_CALIB_H_LEN] = {0};
    /* dig_H4 = 0xFFF (12-bit) = -1, dig_H5 = 0x80F (12-bit) = -2033 */
    h[3] = 0xFFU; /* 0xE4: H4[11:4]                  */
    h[4] = 0xFFU; /* 0xE5: H5[3:0] = 0xF, H4[3:0] = 0xF */
    h[5] = 0x80U; /* 0xE6: H5[11:4]                  */
    h[6] = 0xF6U; /* 0xE7: dig_H6 = -10              */
    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_parse_calibration(tp, h, &calib));
    TEST_ASSERT_EQUAL_INT16(-1, calib.dig_H4);
    TEST_ASSERT_EQUAL_INT16(-2033, calib.dig_H5);
    TEST_ASSERT_EQUAL_INT8(-10, calib.dig_H6);
}

static void test_parse_rejects_null_pointers(void)
{
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR,
                          bme280_parse_calibration(NULL, FIXTURE_CALIB_H, &calib));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR,
                          bme280_parse_calibration(FIXTURE_CALIB_TP, NULL, &calib));
    TEST_ASSERT_EQUAL_INT(BME280_E_NULL_PTR,
                          bme280_parse_calibration(FIXTURE_CALIB_TP, FIXTURE_CALIB_H, NULL));
}

static void test_read_calibration_from_bus(void)
{
    fake_i2c_t bus;
    bme280_hal_t hal;
    bme280_dev_t dev;

    fake_i2c_reset(&bus, BME280_I2C_ADDR_PRIMARY);
    fake_i2c_load_bme280_fixture(&bus);
    hal = fake_i2c_hal(&bus);
    (void)memset(&dev, 0, sizeof(dev));
    dev.hal = &hal;
    dev.addr = BME280_I2C_ADDR_PRIMARY;

    TEST_ASSERT_EQUAL_INT(BME280_OK, bme280_read_calibration(&dev));
    TEST_ASSERT_EQUAL_UINT(2U, bus.read_calls); /* two burst reads, not 33 single reads */
    assert_fixture_calibration(&dev.calib);
}

static void test_read_calibration_propagates_bus_error(void)
{
    fake_i2c_t bus;
    bme280_hal_t hal;
    bme280_dev_t dev;

    fake_i2c_reset(&bus, BME280_I2C_ADDR_PRIMARY);
    bus.fail_reads = 1;
    hal = fake_i2c_hal(&bus);
    (void)memset(&dev, 0, sizeof(dev));
    dev.hal = &hal;
    dev.addr = BME280_I2C_ADDR_PRIMARY;

    TEST_ASSERT_EQUAL_INT(BME280_E_COMM, bme280_read_calibration(&dev));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_temperature_and_pressure_words);
    RUN_TEST(test_parse_unsigned_words_above_32767);
    RUN_TEST(test_parse_humidity_nibbles_negative);
    RUN_TEST(test_parse_rejects_null_pointers);
    RUN_TEST(test_read_calibration_from_bus);
    RUN_TEST(test_read_calibration_propagates_bus_error);
    return UNITY_END();
}
