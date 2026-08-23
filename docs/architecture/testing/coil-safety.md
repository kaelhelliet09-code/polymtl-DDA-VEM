# Coil safety host-test boundary

`coil_safety_tests` compiles the production `GpioPin`, `InternalDacChannel`,
`Drv8874`, and `CoilController` sources against the isolated fake HAL under
`Tests/Fakes/Coil`.

The fake records GPIO, DAC, and timer operations. A direct STM32 BSRR write is
observed as the emergency bridge-disable register command; the host model does
not reproduce the GPIO peripheral's internal propagation from BSRR to ODR.
TIM2 advances one microsecond for each counter read so the test can establish
that both direct reversals and Stop-to-enable transitions remain all-off for
at least the configured 10 microseconds. A stopped TIM2 is also verified to
reject re-enabling without falling back to an interrupt-dependent delay. This
validates software ordering, not oscillator accuracy or real interrupt
latency.

The tests cannot reproduce DRV8874 electrical behavior, IMODE automatic retry,
analog current settling, or EXTI synchronizer latency. Those remain target
scope tests. The software tests do verify that fault acknowledgement performs
no nSLEEP operation: the production clear path only checks drive/current
state, the fault epoch, EXTI pending bits, and live fault inputs before clearing
software latches.
