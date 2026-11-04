# Fake I2C bus shared by the unit tests and the host example.
add_library(bme280_fake_i2c STATIC fake_i2c.c)
target_include_directories(bme280_fake_i2c PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bme280_fake_i2c PUBLIC bme280)
bme280_set_warnings(bme280_fake_i2c)

# Recording stand-in for the STM32 Cube HAL, used to test port/stm32 on a PC.
add_library(bme280_stm32_hal_stub STATIC stm32_hal_stub/stm32_hal_stub.c)
target_include_directories(bme280_stm32_hal_stub PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/stm32_hal_stub")
bme280_set_warnings(bme280_stm32_hal_stub)
