# User-owned firmware

This directory contains all user-maintained C++ firmware. Headers are colocated
with implementations and include paths are rooted at `Application`.

- `App` constructs and services the complete board application.
- `Config` owns compile-time board and policy constants.
- `Drivers` controls individual external devices.
- `Platform/Stm32/Analog` adapts ADC sampling and internal DAC output.
- `Platform/Stm32/Gpio` wraps STM32 GPIO access.
- `Platform/Stm32/I2c` wraps blocking I2C device access.
- `Platform/Stm32/System` adapts interrupt events and HAL timestamps.
- `Platform/Stm32/Usb` owns USB CDC bridging, transport, and recovery.
- `Service` coordinates hardware into application behavior and owns the
  HAL-independent USB wire representation.

Do not place user-owned C++ under the CubeMX-generated `Core` or `USB_DEVICE`
trees. The configure-time repository-layout check rejects those additions.
