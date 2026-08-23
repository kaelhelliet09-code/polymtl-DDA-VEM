# AdcSampler host-test boundary

`adc_sampler_tests` compiles the production `AdcSampler.cpp` against the
minimal HAL implementation in `Tests/Fakes`. The fake controls synchronous
callbacks, start/stop failures, DMA and ADC error state, and pending interrupt
sources. The tests also exercise the explicit startup validation of the fixed
CubeMX ADC/DMA configuration.

The STM32 HAL callback carries only an `ADC_HandleTypeDef*`, not a transfer
generation. A deliberately injected callback after a new owner has already
been published is therefore indistinguishable from that owner's real callback
in both the fake and the target HAL. Production prevents that case by clearing
the DMA terminal flag, ADC terminal flags, and shared NVIC pending bit while
interrupts are masked and before publishing the new owner. The host test
verifies that ordering through a pending-source-triggered fake start, but the
hardware/NVIC behavior still requires target testing.

`AdcSampler` retains its callback-safe, non-blocking sequence interface for a
future run-data collector; it does not expose a blocking polling path.
