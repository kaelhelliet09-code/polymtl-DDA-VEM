"""Programme de lancement du concours réservé à l'organisateur."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import sys
import time
from typing import Callable

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from dda_host import (
    BoardError,
    BoardRejectedError,
    BoardTimeoutError,
    Bridge,
    CompetitionBoard,
    DeviceMode,
    LaunchResult,
    LaunchStatus,
    PowerStageFault,
    SensorCalibration,
    formatTimingReport,
    plotLaunchResult,
)
from examples.competition import participant_code


@dataclass(frozen=True, slots=True)
class CoilTestResult:
    """Recorded launch and fault checks from a technician coil test."""

    launch: LaunchResult
    one_amp_faults: PowerStageFault
    two_amp_faults: PowerStageFault

    @property
    def passed(self) -> bool:
        return (
            self.one_amp_faults == PowerStageFault.NONE
            and self.two_amp_faults != PowerStageFault.NONE
        )


def _fault_names(faults: PowerStageFault) -> str:
    if faults == PowerStageFault.NONE:
        return "aucun"
    fault_labels = {
        PowerStageFault.DRIVER_H1: "pilote H1",
        PowerStageFault.DRIVER_H2: "pilote H2",
        PowerStageFault.DRIVER_H3: "pilote H3",
        PowerStageFault.DRIVER_H4: "pilote H4",
        PowerStageFault.POWER_ALERT: "alerte d'alimentation",
        PowerStageFault.SAFE_STATE_FAILURE: "échec de la mise en sécurité",
    }
    return ", ".join(
        fault_labels.get(fault, fault.name)
        for fault in PowerStageFault
        if fault is not PowerStageFault.NONE and faults & fault
    )


def _status_name(status: LaunchStatus) -> str:
    return {
        LaunchStatus.SUCCESS: "réussi",
        LaunchStatus.TIMED_OUT: "délai dépassé",
        LaunchStatus.HOST_ABORTED: "interrompu par l'hôte",
        LaunchStatus.ACQUISITION_ERROR: "erreur d'acquisition",
        LaunchStatus.SAFETY_FAULT: "défaut de sécurité",
        LaunchStatus.BUSY: "occupé",
        LaunchStatus.INVALID_COMMAND: "commande invalide",
    }[status]


def _print_debug_timings(result: LaunchResult) -> None:
    if result.timing_report is not None:
        print()
        print(formatTimingReport(result.timing_report))


def _exercise_bridges(
    board: CompetitionBoard,
    dwell_s: float,
    report: Callable[[str], None],
) -> None:
    for bridge in (Bridge.H1, Bridge.H2, Bridge.H3, Bridge.H4, Bridge.ALL):
        label = "tous les ponts" if bridge is Bridge.ALL else bridge.name
        report(f"  Activation de {label}...")
        try:
            board.forward(bridge)
        except BoardRejectedError as error:
            report(f"  Activation refusée après un défaut de sécurité : {error}")
            return
        time.sleep(dwell_s)
        try:
            board.off(bridge)
        except BoardRejectedError as error:
            report(
                "  Le pont avait déjà été désactivé par la logique de "
                f"sécurité : {error}"
            )
            return


def run_coil_test(
    board: CompetitionBoard,
    *,
    dwell_s: float = 0.5,
    report: Callable[[str], None] = print,
) -> CoilTestResult:
    """Run the resistor-bar test and return its captured launch data."""

    if dwell_s < 0:
        raise ValueError("dwell_s ne doit pas être négatif")

    board.startLaunch()
    try:
        report("Test de chaque pont à 1 A...")
        board.setCurrent(1000)
        board.wake(Bridge.ALL)
        _exercise_bridges(board, dwell_s, report)
        one_amp_faults = board.getFaults()
        report(f"Défauts à 1 A : {_fault_names(one_amp_faults)}")

        two_amp_faults = PowerStageFault.NONE
        if one_amp_faults == PowerStageFault.NONE:
            report(
                "Test de chaque pont à 2 A ; l'étape avec tous les ponts "
                "doit provoquer un défaut..."
            )
            board.setCurrent(2000)
            _exercise_bridges(board, dwell_s, report)
            two_amp_faults = board.getFaults()
            report(f"Défauts à 2 A : {_fault_names(two_amp_faults)}")
        else:
            report(
                "Test à 2 A ignoré : un défaut inattendu était déjà présent "
                "à 1 A."
            )

        launch = board.stopLaunch()
    except BaseException:
        if board.launch_active:
            try:
                board.abortLaunch()
            except BoardError:
                pass
        raise

    result = CoilTestResult(launch, one_amp_faults, two_amp_faults)
    report(
        "Test des bobines RÉUSSI."
        if result.passed
        else "Test des bobines ÉCHOUÉ."
    )
    return result


def run_sensor_test(
    board: CompetitionBoard,
    *,
    report: Callable[[str], None] = print,
) -> tuple[SensorCalibration, ...]:
    """Calibrate every sensor and print the stored DAC codes."""

    report("Étalonnage de tous les capteurs...")
    calibrations = board.calibrateSensors()
    for calibration in calibrations:
        report(
            f"Capteur {int(calibration.sensor) + 1} : code du courant de la DEL "
            f"{calibration.led_current_code}, code de la tension de seuil "
            f"{calibration.trip_voltage_code}"
        )
    report("Test des capteurs RÉUSSI.")
    return calibrations


def run_participant_code(board: CompetitionBoard) -> LaunchResult | None:
    try:
        return participant_code(board)
    except KeyboardInterrupt:
        print("Interruption du participant ; arrêt sécuritaire du lancement...")
        if board.launch_active:
            try:
                result = board.abortLaunch()
            except KeyboardInterrupt:
                print(
                    "Arrêt interrompu ; fermeture de la connexion afin que la "
                    "carte puisse se réinitialiser en toute sécurité.",
                    file=sys.stderr,
                )
            except BoardError as error:
                print(
                    f"Impossible de confirmer l'arrêt du lancement : {error}. "
                    "La carte suivra sa procédure de réinitialisation sécuritaire.",
                    file=sys.stderr,
                )
            else:
                print(
                    "Lancement interrompu. "
                    f"État final : {_status_name(result.status)}"
                )
        return None
    except BoardTimeoutError:
        if board.launch_status is not LaunchStatus.TIMED_OUT:
            raise
        print(
            "Le lancement a dépassé le délai de 5 secondes. "
            "Code du participant arrêté ; réception des données du lancement..."
        )
        result = board.stopLaunch()
        print(
            "Données du lancement reçues. "
            f"État final : {_status_name(result.status)}"
        )
        return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--port", help="port COM ; omettre pour utiliser la détection automatique"
    )
    parser.add_argument(
        "--competition",
        action="store_true",
        help=(
            "activer le verrouillage normal du concours au lieu du mode "
            "test/débogage"
        ),
    )
    parser.add_argument(
        "--technician-test",
        choices=("coil", "sensor", "all"),
        help="qualifier la carte au lieu d'exécuter le code du participant",
    )
    parser.add_argument(
        "--step-seconds",
        type=float,
        default=0.5,
        help="durée d'activation des bobines par étape (valeur par défaut : 0,5 s)",
    )
    args = parser.parse_args()
    if args.technician_test and args.competition:
        parser.error("les tests du technicien nécessitent le mode débogage")
    mode = DeviceMode.COMPETITION if args.competition else DeviceMode.DEBUG

    try:
        with CompetitionBoard.connect(args.port, mode=mode) as board:
            board.setSamplingFrequency(5000)
            if args.technician_test:
                if args.technician_test in ("sensor", "all"):
                    input(
                        "Installez les capteurs dans l'appareil d'étalonnage, "
                        "puis appuyez sur Entrée."
                    )
                    run_sensor_test(board)
                if args.technician_test in ("coil", "all"):
                    input(
                        "Installez la barre de résistances de test et "
                        "réinitialisez la carte si un défaut est mémorisé, "
                        "puis appuyez sur Entrée."
                    )
                    result = run_coil_test(board, dwell_s=args.step_seconds)
                    _print_debug_timings(result.launch)
                    plotLaunchResult(result.launch)
                return
            board.setCurrent(1000)
            board.useDefaultSensorLevels()
            board.wake(Bridge.ALL)
            try:
                result = run_participant_code(board)
            except BoardError as error:
                print(
                    "Lancement interrompu après une erreur de communication "
                    f"avec la carte : {error}",
                    file=sys.stderr,
                )
                return
            if result is None:
                return
            if result.data is not None:
                print(f"Vitesse : {result.data.velocity_m_s:.2f} m/s")
            _print_debug_timings(result)
            plotLaunchResult(result)
    except KeyboardInterrupt:
        print(
            "Programme interrompu ; la connexion avec la carte a été fermée "
            "en toute sécurité."
        )


if __name__ == "__main__":
    main()
