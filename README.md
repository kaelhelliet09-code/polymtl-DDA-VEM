# DDA board firmware and host tools

This repository contains the STM32G0B1 firmware and the Python organizer tool
for the four-bridge DDA board. The supported host protocol is the three-byte
service protocol implemented by `CompetitionBoard`.

## Repository layout

```text
Application/       User-owned C++ firmware
  App/             Startup and foreground composition
  Config/          Board, power, safety, sensor, storage, and USB settings
  Drivers/         DRV8874, INA226, DAC, and velocity-sensor drivers
  Platform/Stm32/  HAL adapters
  Service/         Launch, power, safety, sensor, UI, and USB logic
Core/              CubeMX startup and peripheral initialization
Drivers/           STM32 HAL and CMSIS
Middlewares/       STM32 USB device middleware
USB_DEVICE/        CubeMX USB CDC integration
Host/Competition/  Minimal participant API and competition runner
Host/Technician/   Full diagnostic API and hardware/software tests
Tests/Firmware/    Board-independent C++ tests
docs/              Focused design and hardware notes
```

`DDA_V2.ioc` is the source of truth for generated peripheral configuration. Keep
application behavior under `Application`; generated changes must stay in
CubeMX user sections or user-owned adapters.

## Safety behavior

The firmware starts with all bridges disabled and the applied current reference
at zero. Driver nFAULT inputs and the INA226 power alert are latched by the
safety service. A fault removes drive immediately and prevents unsafe commands
from restoring it. The external hardware arming system remains the physical
power interlock.

Current commands can update all bridges or one bridge independently, from 0
to 3000 mA in 25 mA steps. A live bridge accepts a new VREF without a sleep
cycle; a sleeping bridge retains the request while its VREF remains at zero.
The INA226 uses a separate 6 A whole-board measurement range; this is not a
software current limit.

## Build and automated tests

Firmware images:

```powershell
cmake --preset Debug
cmake --build --preset Debug

cmake --preset Release
cmake --build --preset Release
```

Board-independent firmware tests:

```powershell
cmake --preset Host
cmake --build --preset Host
ctest --preset Host
```

Python tests:

```powershell
cd Host\Technician
python -m pip install -e .
python -m unittest discover -s tests -v
```

## Technician board qualification

Flash the current firmware, connect the board over USB CDC, install the Python
package as shown above, and run from `Host\Technician`:

```powershell
python test_ponts_h.py
```

The script automatically detects the single attached board. The complete
French procedure is in `Host/Technician/TEST_PONTS_H.md`.

### Coil test

This is an energized hardware test. Use the specified resistor test bar and
the normal protected bench setup.

1. Install the resistor test bar. Reset or power-cycle the board if a previous
   test left a driver fault latched.
2. Start the command and confirm the prompt. The organizer starts one recorded
   launch attempt.
3. At 1 A, it activates H1, H2, H3, H4, then all bridges. Every activation is
   followed by an off command. No fault is expected.
4. The organizer reads and prints the fault mask.
5. At 2 A, it repeats H1, H2, H3, H4, then all bridges. The all-bridge step is
   expected to trip a hardware protection fault.
6. The organizer reads and prints the fault mask, ends the launch, reports
   pass/fail, and opens the recorded current/power/command graph.
7. The technician confirms from the graph that each bridge drove in sequence,
   current measurement is plausible, and the expected protection event
   removed drive.

The test fails if any fault appears at 1 A or no fault appears at 2 A. It does
not clear a latched fault automatically.

The V1 automatic sensor calibration procedure is not supported by this V2
firmware. IR LEDs are GPIO-enabled and have no LED-current DAC code. Sensor
VTRIP uses the fixed default from `SensorConfig.h` until a V2 calibration
procedure is specified.

## Competition runner

Install and run from `Host\Competition`:

```powershell
python run_competition.py
```

Participant code lives in `Host/Competition/competition.py`. The port is
detected automatically, the board is always placed in competition mode, and
the public participant API does not expose diagnostic or test operations.
In competition mode, coil and sensor commands are accepted only while a launch
is active; launch lifecycle commands remain available so a run can be started
and stopped.

## Configuration

Hardware behavior is configured by the headers under
`Application/Config`:

- `BoardConfig.h`: fixed topology, converter characteristics, and TIM2 clock
- `ExternalDacConfig.h`: exclusive VTRIP and driver-VREF channel ownership
- `PowerConfig.h`: INA226 and DRV8874 scaling and current limits
- `SafetyConfig.h`: fault validation and the blocking driver-wake delay
- `SensorConfig.h`: sensor GPIO mapping, VTRIP default, and debounce interval
- `UsbConfig.h`: CDC packet buffers and timeouts

Include only the header that owns a setting. Hardware mappings must remain in
sync with `DDA_V2.ioc`.
