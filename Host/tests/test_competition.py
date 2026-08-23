"""Tests for the current two-thread competition host protocol."""

from __future__ import annotations

import struct
import threading
import time
import unittest
from unittest.mock import patch
import zlib

from dda_host import (
    BoardError,
    BoardProtocolError,
    BoardRejectedError,
    BoardTimeoutError,
    Bobine,
    Bridge,
    Capteur,
    CarteCompetition,
    CompetitionBoard,
    DeviceMode,
    Direction,
    LaunchStatus,
    SensorCalibration,
    SensorChannel,
    SensorEdge,
    formatTimingReport,
    plotLaunchResult,
)


class FakeCompetitionSerial:
    def __init__(self) -> None:
        self.timeout = 0.02
        self.writes: list[bytes] = []
        self.reject_coils = False
        self.corrupt_crc = False
        self.disconnect_on_stop = False
        self._received = bytearray()
        self._condition = threading.Condition()
        self._closed = False

    def write(self, data: bytes) -> int:
        if self._closed:
            raise OSError("USB device disconnected")
        frame = bytes(data)
        self.writes.append(frame)
        service, command, encoded_options = frame
        options = encoded_options & 0x7F
        status = 1 if self.reject_coils and service == 1 else 0

        if service == 3 and command == 1:  # StopRun
            # Unsolicited final status may legally arrive before the command ack.
            self.inject(bytes((3, 5, int(LaunchStatus.SUCCESS))))
            self.inject(bytes((service, command, status)))
            if self.disconnect_on_stop:
                self.close()
            else:
                self._inject_launch_data()
        elif service == 2 and command in (5, 6):
            response = (100 if command == 5 else 150) + options
            self.inject(bytes((service, command, response)))
        else:
            response = options if service == 5 and command in (2, 3) else status
            self.inject(bytes((service, command, response)))
        return len(frame)

    def read(self, size: int = 1) -> bytes:
        deadline = time.monotonic() + (self.timeout or 0.02)
        with self._condition:
            while not self._received and not self._closed:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return b""
                self._condition.wait(remaining)
            if not self._received and self._closed:
                raise OSError("USB device disconnected")
            result = bytes(self._received[:size])
            del self._received[:size]
            return result

    def inject(self, data: bytes) -> None:
        with self._condition:
            self._received.extend(data)
            self._condition.notify_all()

    def _inject_launch_data(self) -> None:
        payload = bytearray()
        payload.extend(struct.pack("<BBII", 1, 1, 70_400, 76_800))
        payload.extend(b"\x00\x00" * 3)
        current_samples = bytes(range(1, 81))
        payload.extend(struct.pack("<I", 20))
        payload.extend(current_samples)
        payload.extend(
            struct.pack(
                "<IIHHIII", 2, 3, 400, 500, 64_000, 128_000, 64_000
            )
        )
        payload.extend(struct.pack("<H", 4))
        payload.extend(
            struct.pack("<BBBBIII", 1, 0, 1, 1, 90_000, 96_000, 97_000)
        )
        payload.extend(
            struct.pack("<BBBBIII", 1, 3, 4, 1, 98_000, 100_000, 101_000)
        )
        payload.extend(
            struct.pack("<BBBBIII", 1, 1, 2, 1, 102_000, 104_000, 105_000)
        )
        payload.extend(
            struct.pack("<BBBBIII", 3, 0, 7, 1, 63_000, 65_000, 66_000)
        )

        run_id = 7
        chunk_index = 0
        for offset in range(0, len(payload), 56):
            chunk = payload[offset : offset + 56]
            header = bytes(
                (0xDA, 4, 0, run_id, chunk_index & 0xFF, chunk_index >> 8,
                 len(chunk), 0)
            )
            self.inject(header + chunk)
            chunk_index += 1
        crc = zlib.crc32(payload) & 0xFFFFFFFF
        if self.corrupt_crc:
            crc ^= 1
        final_payload = struct.pack("<II", len(payload), crc)
        final_header = bytes(
            (0xDA, 4, 1, run_id, chunk_index & 0xFF, chunk_index >> 8, 8, 0)
        )
        self.inject(final_header + final_payload)

    def close(self) -> None:
        with self._condition:
            self._closed = True
            self._condition.notify_all()


class CompetitionBoardTests(unittest.TestCase):
    def test_launch_timeout_unblocks_sensor_wait(self) -> None:
        connection = FakeCompetitionSerial()
        with CompetitionBoard(connection) as board:
            board.startLaunch(4)
            connection.inject(bytes((3, 5, 1)))

            with self.assertRaisesRegex(BoardTimeoutError, "TIMED_OUT"):
                board.waitForSensor(SensorChannel.SENSOR_1, timeout_s=1.0)

    def test_simple_wrappers_send_current_request_frames(self) -> None:
        connection = FakeCompetitionSerial()
        board = CompetitionBoard(connection)
        try:
            board.setMode(DeviceMode.DEBUG)
            board.setCurrent(1000)
            board.wake(Bridge.ALL)
            board.forward(Bridge.H2)
            board.off(Bridge.H2)
            board.unlockSensor()
            board.useDefaultSensorLevels(SensorChannel.ALL)

            self.assertEqual(connection.writes[0], bytes((3, 4, 0x81)))
            self.assertEqual(connection.writes[1], bytes((1, 4, 0xA8)))
            self.assertEqual(connection.writes[2], bytes((1, 3, 0x84)))
            self.assertEqual(connection.writes[3], bytes((1, 0, 0x81)))
            self.assertEqual(connection.writes[4], bytes((1, 6, 0x81)))
            self.assertEqual(connection.writes[5], bytes((2, 4, 0x80)))
            self.assertEqual(connection.writes[6], bytes((2, 2, 0x84)))
        finally:
            board.close()

    def test_french_participant_wrappers_send_expected_frames(self) -> None:
        connection = FakeCompetitionSerial()
        board = CompetitionBoard(connection)
        try:
            board.reglerCourant(1000)
            board.reveiller(Bobine.H1)
            board.activer(Bobine.H1, Direction.AVANT)
            board.activer(Bobine.H2, Direction.ARRIERE)
            board.desactiver(Bobine.H2)
            board.mettreEnVeille(Bobine.H1)

            self.assertEqual(connection.writes[0], bytes((1, 4, 0xA8)))
            self.assertEqual(connection.writes[1], bytes((1, 3, 0x80)))
            self.assertEqual(connection.writes[2], bytes((1, 0, 0x80)))
            self.assertEqual(connection.writes[3], bytes((1, 1, 0x81)))
            self.assertEqual(connection.writes[4], bytes((1, 6, 0x81)))
            self.assertEqual(connection.writes[5], bytes((1, 2, 0x80)))
        finally:
            board.close()

    def test_french_launch_and_sensor_wrappers(self) -> None:
        connection = FakeCompetitionSerial()
        board = CompetitionBoard(connection)
        try:
            self.assertEqual(board.demarrerLancement(7), 7)
            connection.inject(bytes((2, 0, 0x00)))
            notification = board.attendreCapteur(
                Capteur.CAPTEUR_1, delai_s=0.5
            )
            self.assertEqual(notification.capteur, Capteur.CAPTEUR_1)

            result = board.arreterLancement(delai_s=1.0)
            self.assertEqual(result.status, LaunchStatus.SUCCESS)
            self.assertIsNotNone(result.timing_report)
            assert result.timing_report is not None
            self.assertEqual(len(result.timing_report.sensors), 1)
            self.assertEqual(
                result.timing_report.sensors[0].sensor,
                SensorChannel.SENSOR_1,
            )
        finally:
            board.close()

    def test_french_public_type_aliases(self) -> None:
        self.assertIs(CarteCompetition, CompetitionBoard)
        self.assertIs(Capteur, SensorChannel)
        self.assertIs(Bobine, Bridge)

    def test_receiver_thread_records_sensor_edges(self) -> None:
        connection = FakeCompetitionSerial()
        board = CompetitionBoard(connection)
        try:
            connection.inject(bytes((2, 0, 0x06)))  # Sensor 3 falling
            event = board.waitForSensor(timeout_s=0.5)
            self.assertEqual(event.sensor, SensorChannel.SENSOR_3)
            self.assertEqual(event.edge, SensorEdge.FALLING)
        finally:
            board.close()

    def test_calibrates_then_reads_all_sensor_codes(self) -> None:
        connection = FakeCompetitionSerial()
        board = CompetitionBoard(connection)
        try:
            calibrations = board.calibrateSensors()

            self.assertEqual(
                calibrations,
                tuple(
                    SensorCalibration(
                        SensorChannel(index), 100 + index, 150 + index
                    )
                    for index in range(4)
                ),
            )
            self.assertEqual(connection.writes[0], bytes((2, 1, 0x80)))
            self.assertEqual(connection.writes[1], bytes((2, 5, 0x80)))
            self.assertEqual(connection.writes[2], bytes((2, 6, 0x80)))
        finally:
            board.close()

    def test_calibration_read_rejects_all_sensor_selector(self) -> None:
        board = CompetitionBoard(FakeCompetitionSerial())
        try:
            with self.assertRaises(ValueError):
                board.readSensorCalibration(SensorChannel.ALL)
        finally:
            board.close()

    def test_stop_launch_returns_crc_checked_decoded_data(self) -> None:
        connection = FakeCompetitionSerial()
        board = CompetitionBoard(connection)
        try:
            board.startLaunch(run_id=7)
            result = board.stopLaunch(timeout_s=1.0)

            self.assertEqual(result.status, LaunchStatus.SUCCESS)
            self.assertIsNotNone(result.data)
            assert result.data is not None
            self.assertEqual(result.data.duration_us, 1000)
            self.assertEqual(result.data.velocity_tick_delta, 64_000)
            self.assertAlmostEqual(result.data.velocity_m_s, 50.0)
            self.assertEqual(
                result.data.sensor_events[0].rising_timestamps_ticks,
                (70_400,),
            )
            self.assertEqual(
                result.data.sensor_events[0].falling_timestamps_ticks,
                (76_800,),
            )
            self.assertEqual(result.data.current_samples[0], (1, 2, 3, 4))
            self.assertEqual(result.data.current_samples[-1], (77, 78, 79, 80))
            self.assertEqual(result.data.power_samples, (400, 500))
            self.assertEqual(result.data.missed_power_samples, 3)
            self.assertEqual(len(result.data.request_snapshots), 4)
            snapshot = result.data.request_snapshots[0]
            self.assertEqual(snapshot.command, 0)
            self.assertEqual(snapshot.options, 1)
            self.assertEqual(snapshot.created_at_ticks, 90_000)
            self.assertEqual(snapshot.outgoing_at_ticks, 96_000)
            self.assertEqual(snapshot.done_at_ticks, 97_000)
            self.assertIsNotNone(result.timing_report)
            assert result.timing_report is not None
            self.assertEqual(len(result.timing_report.requests), 1)
            timing = result.timing_report.requests[0]
            self.assertGreater(timing.round_trip_us, 0.0)
            self.assertAlmostEqual(timing.device_processing_us, 31.25)
            self.assertAlmostEqual(timing.completion_to_usb_queue_us, 15.625)
            report_text = formatTimingReport(result.timing_report)
            self.assertIn("hôte -> fin du traitement par le périphérique", report_text)
            self.assertIn("LAUNCH:0", report_text)
            self.assertNotIn(bytes((1, 2, 0x84)), connection.writes)
            self.assertNotIn(bytes((3, 6, 0x87)), connection.writes)
            self.assertNotIn(bytes((3, 7, 0x87)), connection.writes)
            with patch("matplotlib.pyplot.show") as show:
                import matplotlib.pyplot as plt

                plt.close("all")
                plotLaunchResult(result)
                figures = [plt.figure(number) for number in plt.get_fignums()]
                self.assertEqual(len(figures), 3)

                overview = next(
                    figure
                    for figure in figures
                    if any(
                        axis.get_title().startswith("Vue d'ensemble du lancement")
                        for axis in figure.axes
                    )
                )
                details = next(
                    figure
                    for figure in figures
                    if any(
                        axis.get_title() == "Courant des ponts en H"
                        for axis in figure.axes
                    )
                )
                latency = next(
                    figure
                    for figure in figures
                    if any(
                        axis.get_title() == "Latence des requêtes"
                        for axis in figure.axes
                    )
                )
                self.assertEqual(len(overview.axes), 2)
                self.assertEqual(len(details.axes), 4)
                self.assertEqual(len(latency.axes), 2)
                self.assertEqual(len(overview.axes[1].collections), 2)

                detail_axes = {axis.get_title(): axis for axis in details.axes}
                self.assertEqual(
                    set(detail_axes),
                    {
                        "Courant des ponts en H",
                        "Puissance de la carte - 3 échantillon(s) manqué(s)",
                        "Déclenchements des capteurs",
                        "Commandes des bobines",
                    },
                )
                self.assertEqual(
                    len(detail_axes["Commandes des bobines"].collections), 2
                )
                self.assertEqual(
                    len(detail_axes["Déclenchements des capteurs"].collections), 2
                )

                legend_labels = {
                    text.get_text()
                    for text in overview.axes[0].get_legend().get_texts()
                }
                self.assertIn("Courant H1", legend_labels)
                self.assertIn("S1 front montant", legend_labels)
                self.assertIn("S1 front descendant", legend_labels)
                self.assertIn("H2 direct", legend_labels)
                self.assertIn("H3 inverse", legend_labels)
                self.assertNotIn("Réglage du courant", legend_labels)
                self.assertNotIn("Puissance de la carte", legend_labels)
                plt.close("all")
            show.assert_called_once()
        finally:
            board.close()

    def test_competition_mode_does_not_produce_host_timing_report(self) -> None:
        connection = FakeCompetitionSerial()
        board = CompetitionBoard(connection)
        try:
            board.setMode(DeviceMode.COMPETITION)
            board.startLaunch(run_id=7)
            result = board.stopLaunch(timeout_s=1.0)

            self.assertIsNone(result.timing_report)
        finally:
            board.close()

    def test_crc_failure_is_not_retried(self) -> None:
        connection = FakeCompetitionSerial()
        connection.corrupt_crc = True
        board = CompetitionBoard(connection)
        try:
            board.startLaunch(run_id=7)
            with self.assertRaisesRegex(BoardProtocolError, "CRC"):
                board.stopLaunch(timeout_s=1.0)
            self.assertNotIn(bytes((3, 6, 0x87)), connection.writes)
        finally:
            board.close()

    def test_disconnect_is_not_recovered(self) -> None:
        connection = FakeCompetitionSerial()
        connection.disconnect_on_stop = True
        board = CompetitionBoard(connection, launch_timeout_s=0.2)
        try:
            board.startLaunch(run_id=7)
            with self.assertRaisesRegex(BoardError, "disconnected"):
                board.stopLaunch(timeout_s=0.2)
            self.assertNotIn(bytes((3, 6, 0x87)), connection.writes)
        finally:
            board.close()

    def test_competition_lockout_status_rejects_coil_wrapper(self) -> None:
        connection = FakeCompetitionSerial()
        connection.reject_coils = True
        board = CompetitionBoard(connection)
        try:
            with self.assertRaises(BoardRejectedError):
                board.forward(Bridge.H1)
        finally:
            board.close()


if __name__ == "__main__":
    unittest.main()
