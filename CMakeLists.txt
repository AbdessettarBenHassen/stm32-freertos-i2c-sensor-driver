cmake_minimum_required(VERSION 3.18)

project(
  bme280_driver
  VERSION 0.1.0
  DESCRIPTION "Portable BME280 I2C driver with FreeRTOS task and STM32 port"
  HOMEPAGE_URL "https://github.com/AbdessettarBenHassen/stm32-freertos-i2c-sensor-driver"
  LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(BME280_IS_TOP_LEVEL ON)
else()
  set(BME280_IS_TOP_LEVEL OFF)
endif()

option(BME280_BUILD_TESTS "Build the host unit tests (Unity)" ${BME280_IS_TOP_LEVEL})
option(BME280_BUILD_EXAMPLES "Build the host simulated example" ${BME280_IS_TOP_LEVEL})
option(BME280_WERROR "Treat compiler warnings as errors" OFF)
option(BME280_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(BME280_FREERTOS_SIM "Run the FreeRTOS task test on the FreeRTOS POSIX simulator (downloads the kernel)" OFF)

if(BME280_SANITIZE AND BME280_FREERTOS_SIM)
  message(FATAL_ERROR "BME280_FREERTOS_SIM uses signals and pthreads: do not combine it with BME280_SANITIZE")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

if(BME280_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# Driver core: portable, no MCU / RTOS dependency
# ---------------------------------------------------------------------------
add_library(bme280 src/bme280.c)
add_library(bme280::bme280 ALIAS bme280)
target_include_directories(bme280 PUBLIC "${PROJECT_SOURCE_DIR}/include")
bme280_set_warnings(bme280 STRICT)

# ---------------------------------------------------------------------------
# Host-only targets
# ---------------------------------------------------------------------------
if(BME280_BUILD_TESTS OR BME280_BUILD_EXAMPLES)
  add_subdirectory(tests/support)
endif()

if(BME280_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(BME280_BUILD_EXAMPLES)
  add_subdirectory(examples/host_simulated)
endif()
