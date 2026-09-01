# AdcSampler host-test boundary

`AdcSampler` validates the fixed CubeMX ADC/DMA configuration at startup. The
board-independent `hardware_revision_tests` exercises the production scan
normalizer and verifies that raw numeric-channel order `H1,H2,H4,H3` becomes
the launch wire order `H1,H2,H3,H4`.

The STM32 HAL callback carries only an `ADC_HandleTypeDef*`, not a transfer
generation. Production prevents stale callback ownership by clearing DMA and
ADC terminal flags plus the shared NVIC pending bit while interrupts are
masked and before publishing the new owner. The native test does not emulate
that hardware/NVIC behavior, so it still requires target validation.

`AdcSampler` retains its callback-safe, non-blocking sequence interface for a
future run-data collector; it does not expose a blocking polling path.
