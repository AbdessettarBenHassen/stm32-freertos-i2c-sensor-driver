# ---------------------------------------------------------------------------
# FreeRTOS POSIX simulator: runs port/freertos/bme280_task.c on Linux.
# Offline: -DFETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL=/path/to/FreeRTOS-Kernel
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
  freertos_kernel
  URL https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/V11.1.0.tar.gz
  URL_HASH SHA256=0e21928b3bcc4f9bcaf7333fb1c8c0299d97e2ec9e13e3faa2c5a7ac8a3bc573
  SOURCE_SUBDIR do-not-add-subdirectory)
FetchContent_MakeAvailable(freertos_kernel)

find_package(Threads REQUIRED)

set(FRT "${freertos_kernel_SOURCE_DIR}")
set(FRT_PORT "${FRT}/portable/ThirdParty/GCC/Posix")

add_library(freertos_posix STATIC
  "${FRT}/tasks.c"
  "${FRT}/queue.c"
  "${FRT}/list.c"
  "${FRT}/timers.c"
  "${FRT}/portable/MemMang/heap_3.c"
  "${FRT_PORT}/port.c"
  "${FRT_PORT}/utils/wait_for_event.c")
target_include_directories(freertos_posix SYSTEM PUBLIC
  "${FRT}/include"
  "${FRT_PORT}"
  "${FRT_PORT}/utils"
  "${CMAKE_CURRENT_SOURCE_DIR}")
# The kernel itself is not ours: compile it in GNU mode, without our warnings.
set_target_properties(freertos_posix PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(freertos_posix PUBLIC Threads::Threads)

add_executable(test_bme280_task_sim
  test_bme280_task_sim.c
  "${PROJECT_SOURCE_DIR}/port/freertos/bme280_task.c")
target_include_directories(test_bme280_task_sim PRIVATE "${PROJECT_SOURCE_DIR}/port/freertos")
target_link_libraries(test_bme280_task_sim PRIVATE bme280 bme280_fake_i2c freertos_posix)
set_target_properties(test_bme280_task_sim PROPERTIES C_EXTENSIONS ON)

add_test(NAME test_bme280_task_sim COMMAND test_bme280_task_sim)
set_tests_properties(test_bme280_task_sim PROPERTIES TIMEOUT 20 LABELS freertos)
