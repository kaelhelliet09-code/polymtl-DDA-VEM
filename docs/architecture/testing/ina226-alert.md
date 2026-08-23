# INA226 alert host-test boundary

`Ina226AlertTest.cpp` compiles the production `I2cDevice.cpp` and `Ina226.cpp`
against a private register-level fake. It checks the configured 5 milliohm
shunt and 6 A range, the resulting 120 W raw limit, big-endian register
transactions, active-low transparent mask configuration, readback validation,
and HAL failure propagation.

The fake cannot establish electrical polarity, threshold accuracy, shunt or
bus scaling, deassertion timing, interrupt latency, or behavior during real
bus faults. Those properties still require target measurements against the
assembled board and the INA226 datasheet.
