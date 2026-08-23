# Request and sensor latency architecture

This document describes how the DDA firmware and Python organizer measure
latency during a launch. The feature is diagnostic: it is exported, analyzed,
printed, and plotted only in debug mode. It does not alter the three-byte
command protocol or the safety behavior of competition mode.

## Goals and measurement boundaries

The timing report answers three primary questions:

1. How long after the host sends a command does the device service finish it?
2. How long does the complete host-to-device-to-host request take?
3. How long after a hardware sensor edge does its notification reach the host?

The host and STM32 do not share a physical clock. Round-trip time is measured
directly with one host clock. One-way values cross clock domains and are
therefore estimates obtained by correlating the beginning and end of the
launch. They are useful for profiling and detecting latency spikes, but should
not be interpreted as synchronized laboratory measurements.

## Debug and competition modes

The firmware starts in competition mode. `CompetitionBoard.connect()` selects
debug mode by default; the runner's `--competition` option explicitly keeps
competition behavior.

| Behavior | Debug mode | Competition mode |
| --- | --- | --- |
| Host coil and sensor commands during a launch | Allowed | Rejected by `RequestManager` |
| Current, power, sensor, velocity LaunchData | Sent | Sent |
| Request snapshots in LaunchData | Sent, up to 256 | Snapshot count is zero |
| Host request timestamps retained for analysis | Yes | No |
| Timing statistics printed | Yes | No |
| Latency plot opened | Yes, when samples match | No |
| Safety interlocks and fault handling | Active | Active |

Debug mode removes the competition command lockout; it does not bypass bridge,
power, communication, or safe-state protections.

## End-to-end architecture

```text
participant / organizer main thread
        |
        | perf_counter_ns(): request sent
        v
Python serial write
        |
        v
USB CDC receive callback -> UsbReceiveAccumulator
        |
        v
USBcontroller -> RequestManager -> destination service
                     |                  |
                     | createdAt        | outgoingAt
                     |                  v
                     +----------- response route -> USBcontroller
                                              |
                                              | doneAt
                                              v
                                      USB CDC response
                                              |
                                              v
Python receiver thread: perf_counter_ns()
```

The host main thread is the only command sender. A background receiver thread
is the only serial reader. This makes the send and receive boundaries
unambiguous and lets synchronous API calls retain precise timestamps without
adding bytes to normal packets.

The device keeps request timing inside the existing `RequestManager`. No new
timer, interrupt, packet type, or request buffer is introduced. `LaunchData`
already has a request-snapshot section, so competition mode is represented by
serializing a zero count rather than changing protocol version 4.

## Device timestamps

TIM2 is a free-running 32-bit counter clocked at 64 MHz. One tick is 15.625 ns
and one microsecond is 64 ticks. Every request snapshot contains:

| Field | Capture point |
| --- | --- |
| `createdAt` | The complete request has been decoded and accepted into `RequestManager` |
| `outgoingAt` | The destination service completed and created a response leg |
| `doneAt` | The final service completed; for host responses, bytes have been queued in `UsbTransport` |

`outgoingAt` is optional because requests without answers have no response
leg. The snapshot flag indicates whether it is valid. The host uses
`outgoingAt` as service completion when present and otherwise uses `doneAt`.

All device durations use unsigned subtraction:

```text
elapsed_ticks = (end_ticks - start_ticks) modulo 2^32
elapsed_us    = elapsed_ticks / 64
```

This handles one TIM2 rollover. At 64 MHz the counter rolls over after about
67.1 seconds, while a normal launch is limited to five seconds.

The snapshot buffer holds 256 records. Once full, later completed requests are
still processed but are not recorded. The host reports a saturation warning
when it receives exactly 256 snapshots.

## Launch reference points and clock correlation

The host uses `time.perf_counter_ns()`, a monotonic high-resolution clock. In a
normal host-stopped run the reference pairs are:

| Host reference | Device reference |
| --- | --- |
| Timestamp immediately before writing START | `LaunchData.launch_start_ticks` |
| Timestamp immediately before writing STOP | `LaunchData.launch_end_ticks` |

If firmware ends the run automatically before STOP is sent, reception of the
unsolicited final run status is the fallback host end reference.

Let `H0` and `H1` be the host references and `D0` and `D1` the device
references. A device timestamp `Dx` is mapped into the host clock with:

```text
host(Dx) = H0 + ticks(D0, Dx) * (H1 - H0) / ticks(D0, D1)
```

Using two anchors compensates for clock-rate error across the run. The report's
"clock reference span error" compares the correlated host span with a nominal
64 MHz device span. It includes asymmetric USB latency at START and STOP, so it
is a diagnostic value rather than a crystal-calibration result.

## Host request capture and matching

For each synchronous debug-mode request, the host retains:

- service, command, and original options;
- timestamp immediately before `serial.write()`;
- timestamp in the receiver thread after the matching three-byte response is
  complete.

After CRC-valid LaunchData arrives, host records and device snapshots are
grouped by `(service, command, options)` and paired in chronological order.
This supports repeated identical commands without adding a sequence number to
the wire protocol.

Device-originated requests, such as sensor notifications, remain visible as
device-only snapshots and cannot be mistaken for host requests because their
destination service is USB. The STOP request is intentionally excluded from
matching: `LaunchManager` freezes the LaunchData snapshot count before STOP
itself completes.

The report counts unmatched host requests, device-only snapshots, unmatched
sensor events, and unmatched notifications. Nonzero counts help identify
snapshot saturation, dropped traffic, or firmware/host version disagreement.

## Sensor latency

Sensor hardware edges are timestamped on the device and stored per sensor and
edge polarity in LaunchData. The receiver thread timestamps every matching
sensor notification as soon as its complete packet is read. It also keeps a
private timing copy, so participant calls such as `waitForSensor()` may consume
the public notification without losing diagnostic data.

At the end of the run, device edges and host notifications are grouped by
sensor and polarity, paired in order, and converted through the same clock
correlation. The resulting value covers device interrupt handling, request
routing, USB queuing and transmission, and host serial reception.

## Reported metrics

The console prints sample count, average, maximum, median, and p95 for:

- host send to device service completion, estimated;
- host send to matching host response, directly measured RTT;
- sensor trigger to host notification, estimated;
- device request acceptance to service completion;
- service completion to USB queue;
- device request acceptance to USB queue;
- USB queue to host response reception, estimated.

It also prints average and maximum values grouped by service/command and by
sensor/edge. `plotLaunchResult()` adds a latency window containing request and
sensor samples when the debug report has matched data.

Programmatic callers receive the same information through
`LaunchResult.timing_report`. `formatTimingReport()` formats the console report.
Both are available from the top-level `dda_host` package.

## Operational use

Flash the matching firmware, install the host package, and run without the
competition flag:

```powershell
cd Host
python -m pip install -e .
python run_competition.py --port COM7
```

The timing table is printed after LaunchData is validated. The existing launch
overview and detail windows open together with the debug latency window.

To exercise production restrictions and suppress telemetry:

```powershell
python run_competition.py --port COM7 --competition
```

## Limitations

- One-way USB latency cannot be measured exactly without clock synchronization
  or hardware timestamp exchange.
- `createdAt` is request-manager acceptance, not the USB peripheral's first-bit
  arrival.
- `doneAt` is response queuing, not USB transmit-complete or host reception.
- START/STOP transport asymmetry biases the affine clock correlation.
- Pairing is chronological within a service/command/options key; the fixed
  protocol has no request sequence identifier.
- The device retains at most 256 completed request snapshots per launch.
- Timing diagnostics never invalidate otherwise valid LaunchData; if reference
  spans are unusable, the host returns the launch result without a report.

These boundaries are intentional. They provide useful system-level latency
diagnostics with minimal firmware and protocol complexity.
