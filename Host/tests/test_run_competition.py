"""Tests for launch-timeout handling in the competition runner."""

from __future__ import annotations

import io
import unittest
from unittest.mock import patch

from dda_host import (
    BoardRejectedError,
    BoardTimeoutError,
    Bridge,
    LaunchResult,
    LaunchStatus,
    PowerStageFault,
    SensorCalibration,
    SensorChannel,
)
from run_competition import run_coil_test, run_participant_code, run_sensor_test


class FakeBoard:
    def __init__(self, status: LaunchStatus | None) -> None:
        self.launch_status = status
        self.launch_active = status is None
        self.stop_calls = 0
        self.abort_calls = 0
        self.result = LaunchResult(LaunchStatus.TIMED_OUT, object())

    def stopLaunch(self) -> LaunchResult:
        self.stop_calls += 1
        return self.result

    def abortLaunch(self) -> LaunchResult:
        self.abort_calls += 1
        return LaunchResult(LaunchStatus.HOST_ABORTED, None)


class FakeTechnicianBoard:
    def __init__(self) -> None:
        self.calls: list[tuple[object, ...]] = []
        self.launch_active = False
        self.current_ma = 0
        self.reject_final_off = False
        self._fault_reads = iter(
            (PowerStageFault.NONE, PowerStageFault.DRIVER_H2)
        )
        self.launch_result = LaunchResult(LaunchStatus.SAFETY_FAULT, object())

    def startLaunch(self) -> None:
        self.calls.append(("start",))
        self.launch_active = True

    def setCurrent(self, current_ma: int) -> None:
        self.calls.append(("current", current_ma))
        self.current_ma = current_ma

    def wake(self, bridge: Bridge) -> None:
        self.calls.append(("wake", bridge))

    def forward(self, bridge: Bridge) -> None:
        self.calls.append(("forward", bridge))

    def off(self, bridge: Bridge) -> None:
        self.calls.append(("off", bridge))
        if (
            self.reject_final_off
            and self.current_ma == 2000
            and bridge is Bridge.ALL
        ):
            raise BoardRejectedError("already faulted")

    def getFaults(self) -> PowerStageFault:
        self.calls.append(("faults",))
        return next(self._fault_reads)

    def stopLaunch(self) -> LaunchResult:
        self.calls.append(("stop",))
        self.launch_active = False
        return self.launch_result

    def abortLaunch(self) -> LaunchResult:
        self.launch_active = False
        return LaunchResult(LaunchStatus.HOST_ABORTED, None)

    def calibrateSensors(self) -> tuple[SensorCalibration, ...]:
        return tuple(
            SensorCalibration(SensorChannel(index), 20 + index, 40 + index)
            for index in range(4)
        )


class CompetitionRunnerTests(unittest.TestCase):
    @patch("run_competition.time.sleep")
    def test_coil_test_runs_both_currents_and_records_expected_fault(
        self, _sleep
    ) -> None:
        board = FakeTechnicianBoard()

        result = run_coil_test(  # type: ignore[arg-type]
            board, report=lambda _message: None
        )

        self.assertTrue(result.passed)
        self.assertIs(result.launch, board.launch_result)
        self.assertEqual(
            [call for call in board.calls if call[0] == "current"],
            [("current", 1000), ("current", 2000)],
        )
        self.assertEqual(
            [call[1] for call in board.calls if call[0] == "forward"],
            [Bridge.H1, Bridge.H2, Bridge.H3, Bridge.H4, Bridge.ALL] * 2,
        )
        self.assertEqual(board.calls[-1], ("stop",))

    def test_sensor_test_reports_saved_values(self) -> None:
        board = FakeTechnicianBoard()
        output: list[str] = []

        calibrations = run_sensor_test(  # type: ignore[arg-type]
            board, report=output.append
        )

        self.assertEqual(len(calibrations), 4)
        self.assertIn("Capteur 1 : code du courant de la DEL 20", output[1])
        self.assertEqual(output[-1], "Test des capteurs RÉUSSI.")

    @patch("run_competition.time.sleep")
    def test_expected_fault_shutdown_does_not_abort_coil_report(
        self, _sleep
    ) -> None:
        board = FakeTechnicianBoard()
        board.reject_final_off = True

        result = run_coil_test(  # type: ignore[arg-type]
            board, report=lambda _message: None
        )

        self.assertTrue(result.passed)
        self.assertEqual(board.calls[-1], ("stop",))

    def test_timeout_stops_participant_then_receives_launch_data(self) -> None:
        board = FakeBoard(LaunchStatus.TIMED_OUT)

        with patch(
            "run_competition.participant_code",
            side_effect=BoardTimeoutError("launch ended"),
        ), patch("sys.stdout", new_callable=io.StringIO) as output:
            result = run_participant_code(board)  # type: ignore[arg-type]

        self.assertIs(result, board.result)
        self.assertEqual(board.stop_calls, 1)
        self.assertIn("Code du participant arrêté", output.getvalue())
        self.assertIn("État final : délai dépassé", output.getvalue())

    def test_unrelated_timeout_is_not_treated_as_launch_end(self) -> None:
        board = FakeBoard(None)

        with patch(
            "run_competition.participant_code",
            side_effect=BoardTimeoutError("USB response timed out"),
        ):
            with self.assertRaises(BoardTimeoutError):
                run_participant_code(board)  # type: ignore[arg-type]

        self.assertEqual(board.stop_calls, 0)

    def test_keyboard_interrupt_aborts_active_launch(self) -> None:
        board = FakeBoard(None)

        with patch(
            "run_competition.participant_code", side_effect=KeyboardInterrupt
        ), patch("sys.stdout", new_callable=io.StringIO) as output:
            result = run_participant_code(board)  # type: ignore[arg-type]

        self.assertIsNone(result)
        self.assertEqual(board.abort_calls, 1)
        self.assertIn("arrêt sécuritaire du lancement", output.getvalue())
        self.assertIn("État final : interrompu par l'hôte", output.getvalue())


if __name__ == "__main__":
    unittest.main()
