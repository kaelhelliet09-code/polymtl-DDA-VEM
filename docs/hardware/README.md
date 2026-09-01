# Hardware references

Local component datasheets are stored under [`datasheets`](datasheets/):

- DAC088S085 external DAC
- DRV8874 motor/coil driver
- INA226 power monitor

The authoritative MCU peripheral and pin configuration is `DDA_V2.ioc` at the
repository root.

## DDA V2 mappings

The single DAC088S085 is shared by all sensors and bridge drivers. Channel
ownership is centralized in `Application/Config/ExternalDacConfig.h`:

| Function | S1 / H1 | S2 / H2 | S3 / H3 | S4 / H4 |
| --- | --- | --- | --- | --- |
| Sensor VTRIP | A | B | F | E |
| DRV8874 VREF | C | D | H | G |

ADC1 fixed-sequence scanning follows numeric channel order: H1 on IN0, H2 on
IN4, H4 on IN11, then H3 on IN18. Firmware normalizes this `H1,H2,H4,H3`
DMA order to `H1,H2,H3,H4` before launch data is exposed.

The generated timer map assigns velocity sensor 1 on PC7 to TIM2 CH4 and
velocity sensor 2 on PC6 to TIM2 CH3; the capture adapter uses those generated
channel identities rather than their numeric sensor order.

PMODE is PC4 and starts high for PWM mode. The DRV8874 latches PMODE when
nSLEEP rises, so a PMODE change is accepted only while all four drivers are in
sleep with VREF outputs at zero; firmware waits the specified tSLEEP interval
before changing PC4. VREF itself is a live analog current-limit input and is
updated without forcing an awake bridge to sleep.

## Unresolved sensor-enable mapping

The V2 requirement calls for four independent IR-LED enable GPIOs, but the
current `DDA_V2.ioc` exposes only `SENSOR_ENA` on PC12. The sensor abstraction
and configuration are per sensor, but all four entries necessarily refer to
PC12 until three additional generated pin aliases are provided. No pins were
invented in handwritten code.
