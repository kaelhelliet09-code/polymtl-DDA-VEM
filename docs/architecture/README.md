# Firmware architecture

User-owned firmware is contained under `Application` and follows four broad
roles:

1. `Platform/Stm32` adapts CubeMX handles, STM32 HAL, interrupts, and USB CDC.
2. `Drivers` implements individual external devices.
3. `Service` coordinates devices into board behavior.
4. `App` constructs the system and owns startup and foreground processing.

The USB protocol and calibration record codec are HAL-independent libraries
shared by firmware and board-independent tests. The generated `Core`,
`USB_DEVICE`, `Drivers`, and `Middlewares` trees retain their CubeMX locations.

Test-boundary notes are under [`testing`](testing/).

Detailed subsystem notes:

- [`request-timing.md`](request-timing.md) describes debug/competition modes,
  request snapshots, host/device clock correlation, latency statistics, and
  visualization.
