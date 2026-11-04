/**
 * @file test_bme280_compensation.c
 * @brief Tests for the integer compensation formulas against known reference values.
 *
 * Temperature / pressure reference: Bosch worked example (dig_T1 = 27504 ...,
 * adc_T = 519888, adc_P = 415148) which gives T = 25.08 degC, t_fine = 128422
 * and p = 100653.27 Pa (datasheet double formula). The 64-bit integer formula
 * returns 25767233 (Q24.8), within 0.02 Pa of the published 25767236.
 *
 * Humidity reference values were cross-checked against the double precision
 * formula of datasheet section 8.1 (agreement better than 0.01 %RH).
 */
#include <string.h>

#include "bme280.h"
#include "unity.h"

static bme280_calib_t calib;

void setUp(void)
{
    (void)memset(&calib, 0, sizeof(calib));
    calib.dig_T1 = 27504U;
    calib.dig_T2 = 26435;
    calib.dig_T3 = -1000;
    calib.dig_P1 = 36477U;
    calib.dig_P2 = -10685;
    calib.dig_P3 = 3024;
    calib.dig_P4 = 2855;
    calib.dig_P5 = 140;
    calib.dig_P6 = -7;
    calib.dig_P7 = 15500;
    calib.dig_P8 = -14600;
    calib.dig_P9 = 6000;
    calib.dig_H1 = 75U;
    calib.dig_H2 = 362;
    calib.dig_H3 = 0U;
    calib.dig_H4 = 313;
    calib.dig_H5 = 50;
    calib.dig_H6 = 30;
}

void tearDown(void)
{
}

/* ---------------------------- temperature -------------------------------- */

static void test_temperature_datasheet_example(void)
{
    int32_t t_fine = 0;
    TEST_ASSERT_EQUAL_INT32(2508, bme280_compensate_temperature(&calib, 519888, &t_fine));
    TEST_ASSERT_EQUAL_INT32(128422, t_fine);
}

static void test_temperature_second_point(void)
{
    int32_t t_fine = 0;
    TEST_ASSERT_EQUAL_INT32(1885, bme280_compensate_temperature(&calib, 500000, &t_fine));
    TEST_ASSERT_EQUAL_INT32(96494, t_fine);
}

static void test_temperature_accepts_null_t_fine(void)
{
    TEST_ASSERT_EQUAL_INT32(2508, bme280_compensate_temperature(&calib, 519888, NULL));
}

static void test_temperature_null_calib_returns_zero(void)
{
    int32_t t_fine = 1234;
    TEST_ASSERT_EQUAL_INT32(0, bme280_compensate_temperature(NULL, 519888, &t_fine));
    TEST_ASSERT_EQUAL_INT32(1234, t_fine);
}

/* ------------------------------ pressure --------------------------------- */

static void test_pressure_datasheet_example(void)
{
    uint32_t p = bme280_compensate_pressure(&calib, 415148, 128422);
    TEST_ASSERT_UINT32_WITHIN(16U, 25767236U, p); /* +/- 0.0625 Pa */
    TEST_ASSERT_EQUAL_UINT32(100653U, p / 256U);
}

static void test_pressure_second_point(void)
{
    uint32_t p = bme280_compensate_pressure(&calib, 400000, 96494);
    TEST_ASSERT_UINT32_WITHIN(16U, 26184692U, p);
}

static void test_pressure_zero_divisor_returns_zero(void)
{
    calib.dig_P1 = 0U; /* makes the internal divisor 0 */
    TEST_ASSERT_EQUAL_UINT32(0U, bme280_compensate_pressure(&calib, 415148, 128422));
}

static void test_pressure_null_calib_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, bme280_compensate_pressure(NULL, 415148, 128422));
}

/* ------------------------------ humidity --------------------------------- */

static void test_humidity_reference_55_percent(void)
{
    uint32_t h = bme280_compensate_humidity(&calib, 30000, 128422);
    TEST_ASSERT_EQUAL_UINT32(56317U, h); /* 54.997 %RH */
}

static void test_humidity_reference_38_percent(void)
{
    uint32_t h = bme280_compensate_humidity(&calib, 27000, 128422);
    TEST_ASSERT_EQUAL_UINT32(39190U, h); /* 38.271 %RH */
}

static void test_humidity_clamped_to_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, bme280_compensate_humidity(&calib, 0, 128422));
}

static void test_humidity_clamped_to_100_percent(void)
{
    TEST_ASSERT_EQUAL_UINT32(102400U, bme280_compensate_humidity(&calib, 65535, 128422));
}

static void test_humidity_null_calib_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, bme280_compensate_humidity(NULL, 30000, 128422));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_temperature_datasheet_example);
    RUN_TEST(test_temperature_second_point);
    RUN_TEST(test_temperature_accepts_null_t_fine);
    RUN_TEST(test_temperature_null_calib_returns_zero);
    RUN_TEST(test_pressure_datasheet_example);
    RUN_TEST(test_pressure_second_point);
    RUN_TEST(test_pressure_zero_divisor_returns_zero);
    RUN_TEST(test_pressure_null_calib_returns_zero);
    RUN_TEST(test_humidity_reference_55_percent);
    RUN_TEST(test_humidity_reference_38_percent);
    RUN_TEST(test_humidity_clamped_to_zero);
    RUN_TEST(test_humidity_clamped_to_100_percent);
    RUN_TEST(test_humidity_null_calib_returns_zero);
    return UNITY_END();
}
