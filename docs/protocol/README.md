# USB protocol

Normal requests and responses are three bytes: service, command, and options.
Bit 7 of request options asks for a response; the remaining seven bits carry
the request value. Response options may use all eight bits. LaunchData uses its
separate chunked, CRC-protected framing.

Firmware framing lives in `Application/Service/Usb`; the supported Python
implementation lives in `Host/src/dda_host/competition.py`. Keep service and
command values synchronized between those implementations.

LaunchData protocol version 4 includes a trailing request-snapshot count and
zero or more 16-byte request records. Debug mode exports the records;
competition mode sends a zero count without changing the frame version. See
the [request and sensor latency architecture](../architecture/request-timing.md)
for timestamp semantics, clock correlation, and host reporting.
