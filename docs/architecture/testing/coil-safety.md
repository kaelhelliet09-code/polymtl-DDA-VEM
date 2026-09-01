# Hardware-revision host-test boundary

`hardware_revision_tests` compiles the HAL-independent production headers. It
checks the exclusive eight-channel DAC map, representative current-to-VREF
codes, appended coil-protocol command IDs, ADC scan normalization, and sensor
debounce behavior including opposite-polarity short pulses and timer wrap.

The test does not emulate DRV8874 electrical behavior, SPI transactions, GPIO
propagation, EXTI synchronization, or analog current settling. PMODE latching,
VREF response, fault isolation, and physical sensor pin routing therefore
remain target-scope checks.
