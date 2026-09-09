"""Read velocity-capture events from the DDA Eval Board."""

from __future__ import annotations

import struct

import serial
from serial.tools import list_ports

VID = 0x0483
PID = 0x5741
START = b"\x01"
STOP = b"\x02"
MARKER = 0x56
FRAME_SIZE = 13


def find_eval_board() -> str:
    ports = [
        port.device
        for port in list_ports.comports()
        if port.vid == VID and port.pid == PID
    ]
    if not ports:
        raise RuntimeError("Aucune Eval Board DDA detectee (VID 0483, PID 5741).")
    if len(ports) > 1:
        raise RuntimeError(f"Plusieurs Eval Boards detectees: {', '.join(ports)}")
    return ports[0]


def run() -> None:
    port = find_eval_board()
    print(f"Eval Board detectee sur {port}. Acquisition en cours (Ctrl+C pour arreter).")

    with serial.Serial(port, 115200, timeout=0.2, write_timeout=1.0) as board:
        board.reset_input_buffer()
        board.write(START)
        received = bytearray()

        try:
            while True:
                received.extend(board.read(board.in_waiting or 1))

                while received:
                    if received[0] != MARKER:
                        del received[0]
                        continue
                    if len(received) < FRAME_SIZE:
                        break

                    _, first, second, velocity = struct.unpack(
                        "<BIIf", received[:FRAME_SIZE]
                    )
                    del received[:FRAME_SIZE]
                    delta = (second - first) & 0xFFFFFFFF
                    print(
                        f"t1={first:10d} ticks  t2={second:10d} ticks  "
                        f"delta={delta:10d} ticks  vitesse={velocity:.3f} m/s",
                        flush=True,
                    )
        except KeyboardInterrupt:
            print("\nArret de l'acquisition.")
        finally:
            board.write(STOP)
            board.flush()


if __name__ == "__main__":
    run()
