# USB protocol

Normal requests and responses are three bytes: service, command, and options.
Bit 7 of request options asks for a response; the remaining seven bits carry
the request value. Response options may use all eight bits. LaunchData uses its
separate chunked, CRC-protected framing.

Firmware framing lives in `Application/Service/Usb`; the supported Python
implementation lives in `Host/src/dda_host/competition.py`. Keep service and
command values synchronized between those implementations.

## Coil service additions for DDA V2

Service `1` retains command values `0` through `6` unchanged. The V2 commands
are appended so existing clients remain wire-compatible. Current values use
25 mA per option unit and are validated against the 0–3000 mA range.

| Command | Value | Request options | Response options |
| --- | ---: | --- | --- |
| `SetCurrent` | 4 | Current units for all H1–H4 | `0` success, `1` failure |
| `SetCurrentH1` … `SetCurrentH4` | 7–10 | Current units for the encoded bridge | `0` success, `1` failure |
| `GetCurrentH1` … `GetCurrentH4` | 11–14 | `0` | Configured current units |
| `SetPmode` | 15 | `0` PH/EN, `1` PWM | `0` success, `1` failure |
| `GetPmode` | 16 | `0` | `0` PH/EN, `1` PWM |

An individual current setter encodes the bridge in the command because the
options byte carries the current. A sleeping bridge retains the configured
value while its physical VREF stays at zero; an awake bridge updates VREF
immediately. `SetPmode` is rejected unless every driver is asleep and all four
VREF outputs are zero, because PMODE is latched on the next nSLEEP rising edge.

The supported Python methods are `setCurrent(current_ma, bridge=Bridge.ALL)`,
`getCurrent(bridge)`, `setPmode(pwm_mode)`, and `getPmode()`; snake-case and
French aliases are also provided.

LaunchData protocol version 4 includes a trailing request-snapshot count and
zero or more 16-byte request records. Debug mode exports the records;
competition mode sends a zero count without changing the frame version. See
the [request and sensor latency architecture](../architecture/request-timing.md)
for timestamp semantics, clock correlation, and host reporting.
