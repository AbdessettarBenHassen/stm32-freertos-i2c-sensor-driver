# NUCLEO-L476RG example

Reads a BME280 every second from a FreeRTOS task and prints the values on the
ST-LINK virtual COM port (USART2, 115200 8N1).

## CubeMX / STM32CubeIDE settings

| Peripheral | Setting |
|---|---|
| I2C1 | I2C mode, Fast Mode 400 kHz, PB8 = SCL (Arduino D15), PB9 = SDA (Arduino D14) |
| USART2 | Asynchronous, 115200 baud, 8N1 (PA2 / PA3, routed to ST-LINK) |
| Middleware | FreeRTOS (CMSIS_V2 or native), heap_4, total heap >= 8 KB |
| SYS | Timebase source: TIM6 (keep SysTick for FreeRTOS) |

> PB8/PB9 need to be remapped from the default PB6/PB7 in the CubeMX pinout view.

## Integration

1. Copy `include/`, `src/`, `port/stm32/`, `port/freertos/` into the project (or link them).
2. Add `src/bme280.c`, `port/stm32/bme280_stm32.c`, `port/freertos/bme280_task.c`
   and this `app_main.c` to the build.
3. Add the include paths `include`, `port/stm32`, `port/freertos`.
4. Add the preprocessor define `BME280_STM32_USE_FREERTOS=1`.
   On an STM32F4 board also define `BME280_STM32_HAL_HEADER="stm32f4xx_hal.h"`.
5. In `main.c`, call `app_main();` in `/* USER CODE BEGIN 2 */`, before the scheduler starts.

## Expected output

```
[1000] T=25.08 C  P=1006.53 hPa  H=54.99 %
[2000] T=25.09 C  P=1006.52 hPa  H=55.01 %
```
