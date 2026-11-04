# ---------------------------------------------------------------------------
# Unity test framework
#
# Downloaded once as a pinned, checksummed release tarball. To build fully
# offline, point CMake at a local copy:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_UNITY=/path/to/Unity
# ---------------------------------------------------------------------------
include(FetchContent)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
  unity
  URL https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.6.0.tar.gz
  URL_HASH SHA256=aa4c9fb1ae5fc5242f914c65f3557e817e40cb37f04a31e5ff76d1ab89dbf674
  # Only fetch the sources: we build unity.c ourselves with our own flags.
  SOURCE_SUBDIR do-not-add-subdirectory)
FetchContent_MakeAvailable(unity)

add_library(unity STATIC "${unity_SOURCE_DIR}/src/unity.c")
target_include_directories(unity SYSTEM PUBLIC "${unity_SOURCE_DIR}/src")
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_PRINT_FORMATTED)

# ---------------------------------------------------------------------------
# One executable per test file, one CTest entry per executable
# ---------------------------------------------------------------------------
function(bme280_add_test name)
  add_executable(${name} ${name}.c)
  target_link_libraries(${name} PRIVATE bme280 bme280_fake_i2c unity)
  bme280_set_warnings(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES TIMEOUT 10 LABELS unit)
endfunction()

bme280_add_test(test_bme280_regs)
bme280_add_test(test_bme280_init)
bme280_add_test(test_bme280_calib)
bme280_add_test(test_bme280_compensation)
bme280_add_test(test_bme280_data)

# STM32 binding compiled for the host against the Cube HAL stub.
add_library(bme280_port_stm32_host STATIC "${PROJECT_SOURCE_DIR}/port/stm32/bme280_stm32.c")
target_include_directories(bme280_port_stm32_host PUBLIC "${PROJECT_SOURCE_DIR}/port/stm32")
target_compile_definitions(bme280_port_stm32_host PUBLIC "BME280_STM32_HAL_HEADER=\"stm32_hal_stub.h\"")
target_link_libraries(bme280_port_stm32_host PUBLIC bme280 bme280_stm32_hal_stub)
bme280_set_warnings(bme280_port_stm32_host STRICT)

bme280_add_test(test_bme280_stm32_port)
target_link_libraries(test_bme280_stm32_port PRIVATE bme280_port_stm32_host)

# Optional: run the FreeRTOS task on the FreeRTOS POSIX simulator.
if(BME280_FREERTOS_SIM)
  add_subdirectory(freertos_sim)
endif()
