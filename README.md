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
  Service/         Launch, power, safety, sensor, UI, USB, and calibration logic
Core/              CubeMX startup and peripheral initialization
Drivers/           STM32 HAL and CMSIS
Middlewares/       STM32 USB device middleware
USB_DEVICE/        CubeMX USB CDC integration
Host/              Python organizer API, runner, and tests
Tests/Firmware/    Board-independent C++ tests
docs/              Focused design and hardware notes
```

`DDA.ioc` is the source of truth for generated peripheral configuration. Keep
application behavior under `Application`; generated changes must stay in
CubeMX user sections or user-owned adapters.

## Safety behavior

The firmware starts with all bridges disabled and the applied current reference
at zero. Driver nFAULT inputs and the INA226 power alert are latched by the
safety service. A fault removes drive immediately and prevents unsafe commands
from restoring it. The external hardware arming system remains the physical
power interlock.

Current commands are per bridge, from 0 to 3000 mA in 25 mA steps. The INA226
uses a separate 6 A whole-board measurement range; this is not a software
current limit.

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
cd Host
python -m pip install -e .
python -m unittest discover -s tests -v
```

## Technician board qualification

Flash the current firmware, connect the board over USB CDC, install the Python
package as shown above, and run from `Host`:

```powershell
python run_competition.py --port COM7 --technician-test all
```

Use `--technician-test coil` or `--technician-test sensor` to run only one
procedure. Omit `--port` when exactly one supported board is attached.

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

### Sensor test

1. Put all four sensors in the calibration apparatus.
2. Start the command and confirm the prompt.
3. The firmware disables all bridges, sweeps and calibrates each sensor, saves
   the result, and returns only after the complete calibration succeeds.
4. The organizer reads back and prints each sensor's LED-current DAC code and
   comparator-trip DAC code. Missing or failed calibration stops the test.

## Competition runner

Technician tests and normal competition execution share the same organizer
connection path. Normal debug execution remains:

```powershell
python run_competition.py --port COM7
```

Participant code lives in `Host/examples/competition.py`. Add `--competition`
to exercise the firmware's normal during-launch command lockout.

The two modes deliberately differ during a launch:

| Mode | Coil/sensor commands | Request timing telemetry |
| --- | --- | --- |
| Debug, the runner default | Allowed subject to normal safety checks | Printed and plotted after the run |
| Competition, `--competition` | Rejected while the launch is active | Not exported or analyzed |

Both modes keep all hardware safety interlocks active and return recorded
current, power, sensor, and velocity data. See the
[request and sensor latency architecture](docs/architecture/request-timing.md)
for the complete data flow, timestamp definitions, statistics, and measurement
limitations.

## Configuration

Configuration is intentionally limited to six headers under
`Application/Config`:

- `BoardConfig.h`: fixed topology, converter characteristics, and TIM2 clock
- `PowerConfig.h`: INA226 and DRV8874 scaling and current limits
- `SafetyConfig.h`: fault validation, wake delay, and direction dead time
- `SensorConfig.h`: sensor mapping, DAC values, calibration, and spacing
- `StorageConfig.h`: application and calibration Flash regions
- `UsbConfig.h`: CDC packet buffers and timeouts

Include only the header that owns a setting. Flash layout values must remain in
sync with `linker/STM32G0B1_DDA.ld`, and hardware mappings must remain in sync
with `DDA.ioc`.
