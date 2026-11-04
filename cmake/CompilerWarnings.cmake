# bme280_set_warnings(<target> [STRICT])
#
# Common warning set for GCC and Clang. STRICT adds conversion warnings,
# which matter on 8/16/32-bit MCUs where implicit narrowing hides bugs.
function(bme280_set_warnings target)
  cmake_parse_arguments(ARG "STRICT" "" "" ${ARGN})

  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(warnings
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wcast-align
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wundef
        -Wdouble-promotion)
    if(ARG_STRICT)
      list(APPEND warnings -Wconversion -Wsign-conversion)
    endif()
    if(BME280_WERROR)
      list(APPEND warnings -Werror)
    endif()
  elseif(MSVC)
    set(warnings /W4)
    if(BME280_WERROR)
      list(APPEND warnings /WX)
    endif()
  endif()

  target_compile_options(${target} PRIVATE ${warnings})
endfunction()
