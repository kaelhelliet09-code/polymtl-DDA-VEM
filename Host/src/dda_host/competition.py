"""Two-thread host API for competition and competition-test programs."""

from __future__ import annotations

from collections import defaultdict, deque
from dataclasses import dataclass, field
from enum import Enum, IntEnum, IntFlag
import statistics
import struct
import threading
import time
from typing import ClassVar, Optional, Protocol
import zlib

from .errors import (
    BoardError,
    BoardProtocolError,
    BoardRejectedError,
    BoardTimeoutError,
)

STM32_VID = 0x0483
STM32_CDC_PID = 0x5740
_ANSWER_BIT = 0x80
_OPTION_MASK = 0x7F
_LAUNCH_MARKER = 0xDA
_LAUNCH_VERSION = 4
_LAUNCH_DATA_FRAME = 0
_LAUNCH_FINAL_FRAME = 1
_LAUNCH_HEADER_SIZE = 8
_CURRENT_STEP_MA = 25
_SENSOR_CAPACITY = 10
_SNAPSHOT_CAPACITY = 256
_TIM2_FREQUENCY_HZ = 64_000_000
_VELOCITY_SENSOR_SPACING_METERS = 0.050
_USB_FRAME_TIMEOUT_S = 1.0
_BRIDGE_CURRENT_AMPS_PER_ADC_CODE = 3.3 / (255.0 * 455.0e-6 * 1800.0)
_INA226_CALIBRATION_REGISTER = 5592
_POWER_LSB_W = 25.0 * (0.00512 / (_INA226_CALIBRATION_REGISTER * 0.005))
_CALIBRATION_TIMEOUT_S = 10.0
_CALIBRATION_VALUE_UNAVAILABLE = 0xFF


class PowerStageFault(IntFlag):
    """Power-stage fault bits reported by the current firmware."""

    NONE = 0
    DRIVER_H1 = 1 << 0
    DRIVER_H2 = 1 << 1
    DRIVER_H3 = 1 << 2
    DRIVER_H4 = 1 << 3
    POWER_ALERT = 1 << 4
    SAFE_STATE_FAILURE = 1 << 6

    AUCUN = 0
    PILOTE_H1 = 1 << 0
    PILOTE_H2 = 1 << 1
    PILOTE_H3 = 1 << 2
    PILOTE_H4 = 1 << 3
    ALERTE_PUISSANCE = 1 << 4
    ECHEC_ETAT_SECURITAIRE = 1 << 6


DefautEtagePuissance = PowerStageFault


class Service(IntEnum):
    COILS = 1
    SENSORS = 2
    LAUNCH = 3
    USB = 4
    SAFETY = 5


class Bobine(IntEnum):
    H1 = 0
    H2 = 1
    H3 = 2
    H4 = 3
    TOUTES = 4

    # Nom anglais conserve pour la compatibilite.
    ALL = 4


# Nom anglais conserve pour la compatibilite avec l'API existante.
Bridge = Bobine


class Direction(str, Enum):
    AVANT = "forward"
    ARRIERE = "reverse"

    # Noms anglais conserves pour la compatibilite.
    FORWARD = "forward"
    REVERSE = "reverse"


class Capteur(IntEnum):
    CAPTEUR_1 = 0
    CAPTEUR_2 = 1
    CAPTEUR_3 = 2
    CAPTEUR_4 = 3
    TOUS = 4

    # Noms anglais conserves pour la compatibilite.
    SENSOR_1 = 0
    SENSOR_2 = 1
    SENSOR_3 = 2
    SENSOR_4 = 3
    ALL = 4


SensorChannel = Capteur


class FrontCapteur(str, Enum):
    MONTANT = "rising"
    DESCENDANT = "falling"

    # Noms anglais conserves pour la compatibilite.
    RISING = "rising"
    FALLING = "falling"


SensorEdge = FrontCapteur


def _sensor_edge_label(edge: SensorEdge) -> str:
    return "front montant" if edge is SensorEdge.RISING else "front descendant"


class DeviceMode(IntEnum):
    COMPETITION = 0
    DEBUG = 1


class SystemState(IntEnum):
    INITIALIZING = 0
    SAFE = 1
    READY = 2
    FAULTED = 3

    INITIALISATION = 0
    SECURITAIRE = 1
    PRET = 2
    EN_DEFAUT = 3


ModeCarte = DeviceMode
EtatSysteme = SystemState


class LaunchStatus(IntEnum):
    SUCCESS = 0
    TIMED_OUT = 1
    HOST_ABORTED = 2
    ACQUISITION_ERROR = 3
    SAFETY_FAULT = 4
    BUSY = 5
    INVALID_COMMAND = 6

    REUSSI = 0
    DELAI_DEPASSE = 1
    ANNULE_PAR_HOTE = 2
    ERREUR_ACQUISITION = 3
    DEFAUT_SECURITE = 4
    OCCUPE = 5
    COMMANDE_INVALIDE = 6


EtatLancement = LaunchStatus


def _launch_status_label(status: LaunchStatus) -> str:
    return {
        LaunchStatus.SUCCESS: "réussi",
        LaunchStatus.TIMED_OUT: "délai dépassé",
        LaunchStatus.HOST_ABORTED: "interrompu par l'hôte",
        LaunchStatus.ACQUISITION_ERROR: "erreur d'acquisition",
        LaunchStatus.SAFETY_FAULT: "défaut de sécurité",
        LaunchStatus.BUSY: "occupé",
        LaunchStatus.INVALID_COMMAND: "commande invalide",
    }[status]


class _CoilCommand(IntEnum):
    FORWARD = 0
    REVERSE = 1
    SLEEP = 2
    WAKE = 3
    SET_CURRENT = 4
    GET_FAULTS = 5
    COIL_OFF = 6
    SET_CURRENT_H1 = 7
    SET_CURRENT_H2 = 8
    SET_CURRENT_H3 = 9
    SET_CURRENT_H4 = 10
    GET_CURRENT_H1 = 11
    GET_CURRENT_H2 = 12
    GET_CURRENT_H3 = 13
    GET_CURRENT_H4 = 14
    SET_PMODE = 15
    GET_PMODE = 16


class _SensorCommand(IntEnum):
    REPORT_SENSOR = 0
    CALIBRATE = 1
    SET_DEFAULT_LEVELS = 2
    SET_CALIBRATED_LEVELS = 3
    UNLOCK_SENSOR = 4
    READ_CALIBRATION_LED_CODE = 5
    READ_CALIBRATION_TRIP_CODE = 6


class _LaunchCommand(IntEnum):
    START = 0
    STOP = 1
    ABORT = 2
    SET_SAMPLING_RATE = 3
    SET_DEBUG_MODE = 4
    RUN_STATUS = 5


class _SafetyCommand(IntEnum):
    REPORT_FAULT = 1
    GET_STATE = 2
    GET_FAULTS = 3


@dataclass(frozen=True, slots=True)
class SensorNotification:
    sensor: SensorChannel
    edge: SensorEdge
    received_at_s: float

    @property
    def capteur(self) -> Capteur:
        return self.sensor

    @property
    def front(self) -> FrontCapteur:
        return self.edge

    @property
    def recu_a_s(self) -> float:
        return self.received_at_s


@dataclass(frozen=True, slots=True)
class SensorCalibration:
    """Stored external-DAC codes produced by one sensor calibration."""

    sensor: SensorChannel
    led_current_code: int
    trip_voltage_code: int

    @property
    def capteur(self) -> Capteur:
        return self.sensor

    @property
    def code_courant_del(self) -> int:
        return self.led_current_code

    @property
    def code_seuil_declenchement(self) -> int:
        return self.trip_voltage_code


@dataclass(frozen=True, slots=True)
class SensorEvents:
    rising_timestamps_ticks: tuple[int, ...]
    falling_timestamps_ticks: tuple[int, ...]

    @property
    def rising_timestamps_us(self) -> tuple[float, ...]:
        return tuple(value / 64.0 for value in self.rising_timestamps_ticks)

    @property
    def falling_timestamps_us(self) -> tuple[float, ...]:
        return tuple(value / 64.0 for value in self.falling_timestamps_ticks)

    @property
    def horodatages_fronts_montants_us(self) -> tuple[float, ...]:
        return self.rising_timestamps_us

    @property
    def horodatages_fronts_descendants_us(self) -> tuple[float, ...]:
        return self.falling_timestamps_us


@dataclass(frozen=True, slots=True)
class RequestSnapshot:
    """One 16-byte firmware request record expressed in raw TIM2 ticks."""

    HAS_OUTGOING_TIMESTAMP: ClassVar[int] = 1

    service: Service
    command: int
    options: int
    created_at_ticks: int
    outgoing_at_ticks: Optional[int]
    done_at_ticks: int

    @property
    def execution_timestamp_ticks(self) -> int:
        """Closest recorded timestamp to completion of service processing."""

        return (
            self.done_at_ticks
            if self.outgoing_at_ticks is None
            else self.outgoing_at_ticks
        )


@dataclass(frozen=True, slots=True)
class LatencyStatistics:
    """Aggregate distribution values for one latency metric."""

    count: int
    average_us: float
    maximum_us: float
    median_us: float
    p95_us: float


@dataclass(frozen=True, slots=True)
class RequestLatency:
    """Matched host/device timing values for one debug-mode request."""

    service: Service
    command: int
    options: int
    host_to_completion_us: float
    round_trip_us: float
    device_processing_us: float
    completion_to_usb_queue_us: float
    device_total_us: float
    usb_queue_to_host_us: float


@dataclass(frozen=True, slots=True)
class SensorLatency:
    """Clock-correlated latency for one sensor notification."""

    sensor: SensorChannel
    edge: SensorEdge
    trigger_to_host_us: float


@dataclass(frozen=True, slots=True)
class LaunchTimingReport:
    """Debug-only matched samples, diagnostics, and clock-fit information."""

    requests: tuple[RequestLatency, ...]
    sensors: tuple[SensorLatency, ...]
    device_only_snapshots: int
    unmatched_host_requests: int
    unmatched_sensor_events: int
    unmatched_sensor_notifications: int
    snapshot_capacity_reached: bool
    reference_span_error_ppm: float


# Compatibility name retained for callers using the previous host API.
RequestEvent = RequestSnapshot


@dataclass(frozen=True, slots=True)
class LaunchData:
    """Decoded current, power, sensor, timing, and request data for one run."""

    run_id: int
    sampling_frequency_hz: int
    sensor_events: tuple[SensorEvents, ...]
    current_samples: tuple[tuple[int, int, int, int], ...]
    power_samples: tuple[int, ...]
    missed_power_samples: int
    launch_start_ticks: int
    launch_end_ticks: int
    velocity_tick_delta: int
    request_snapshots: tuple[RequestSnapshot, ...]

    @property
    def request_events(self) -> tuple[RequestSnapshot, ...]:
        """Compatibility alias for the previous launch-data field name."""

        return self.request_snapshots

    @property
    def launch_start_us(self) -> float:
        return self.launch_start_ticks / 64.0

    @property
    def launch_end_us(self) -> float:
        return self.launch_end_ticks / 64.0

    @property
    def duration_us(self) -> float:
        """Return the TIM2 duration, including one possible counter rollover."""

        ticks = (self.launch_end_ticks - self.launch_start_ticks) & 0xFFFFFFFF
        return ticks / 64.0

    @property
    def velocity_m_s(self) -> float:
        """Return the captured projectile velocity in metres per second."""

        if self.velocity_tick_delta == 0:
            return 0.0
        elapsed_seconds = self.velocity_tick_delta / _TIM2_FREQUENCY_HZ
        return _VELOCITY_SENSOR_SPACING_METERS / elapsed_seconds

    @property
    def identifiant(self) -> int:
        return self.run_id

    @property
    def frequence_echantillonnage_hz(self) -> int:
        return self.sampling_frequency_hz

    @property
    def evenements_capteurs(self) -> tuple[SensorEvents, ...]:
        return self.sensor_events

    @property
    def duree_us(self) -> float:
        return self.duration_us

    @property
    def vitesse_m_s(self) -> float:
        return self.velocity_m_s


@dataclass(frozen=True, slots=True)
class LaunchResult:
    """Final launch status, optional captured data, and debug timing report."""

    status: LaunchStatus
    data: Optional[LaunchData]
    timing_report: Optional[LaunchTimingReport] = None

    @property
    def etat(self) -> LaunchStatus:
        return self.status

    @property
    def donnees(self) -> Optional[LaunchData]:
        return self.data

    @property
    def rapport_temporisation(self) -> Optional[LaunchTimingReport]:
        return self.timing_report


# Noms francais des objets retournes par l'API des participants.
NotificationCapteur = SensorNotification
EtalonnageCapteur = SensorCalibration
EvenementsCapteur = SensorEvents
LatenceCapteur = SensorLatency
DonneesLancement = LaunchData
ResultatLancement = LaunchResult
RapportTemporisationLancement = LaunchTimingReport


def _latency_statistics(values: list[float]) -> Optional[LatencyStatistics]:
    if not values:
        return None
    ordered = sorted(values)
    p95_index = max(0, min(len(ordered) - 1, (95 * len(ordered) + 99) // 100 - 1))
    return LatencyStatistics(
        len(ordered),
        statistics.fmean(ordered),
        ordered[-1],
        statistics.median(ordered),
        ordered[p95_index],
    )


def formatTimingReport(report: LaunchTimingReport) -> str:
    """Format a debug launch timing report for the organizer console."""

    lines = ["Statistiques de temporisation du débogage (microsecondes)"]

    def add_summary(label: str, values: list[float]) -> None:
        summary = _latency_statistics(values)
        if summary is None:
            lines.append(f"  {label} : aucun échantillon apparié")
            return
        lines.append(
            f"  {label} : n={summary.count}, moy.={summary.average_us:.2f}, "
            f"max.={summary.maximum_us:.2f}, médiane={summary.median_us:.2f}, "
            f"p95={summary.p95_us:.2f}"
        )

    add_summary(
        "hôte -> fin du traitement par le périphérique",
        [sample.host_to_completion_us for sample in report.requests],
    )
    add_summary(
        "hôte -> fin du traitement par le périphérique -> hôte (RTT)",
        [sample.round_trip_us for sample in report.requests],
    )
    add_summary(
        "déclenchement du capteur -> hôte",
        [sample.trigger_to_host_us for sample in report.sensors],
    )
    add_summary(
        "acceptation par le périphérique -> fin du traitement",
        [sample.device_processing_us for sample in report.requests],
    )
    add_summary(
        "fin du traitement -> file USB",
        [sample.completion_to_usb_queue_us for sample in report.requests],
    )
    add_summary(
        "acceptation par le périphérique -> file USB",
        [sample.device_total_us for sample in report.requests],
    )
    add_summary(
        "file USB -> hôte",
        [sample.usb_queue_to_host_us for sample in report.requests],
    )

    grouped_requests: dict[tuple[Service, int], list[RequestLatency]] = defaultdict(list)
    for sample in report.requests:
        grouped_requests[(sample.service, sample.command)].append(sample)
    if grouped_requests:
        lines.append("Par requête (hôte->fin / RTT moy.,max.) :")
        for (service, command), samples in sorted(
            grouped_requests.items(), key=lambda item: (int(item[0][0]), item[0][1])
        ):
            completion = _latency_statistics(
                [sample.host_to_completion_us for sample in samples]
            )
            round_trip = _latency_statistics([sample.round_trip_us for sample in samples])
            assert completion is not None and round_trip is not None
            lines.append(
                f"  {service.name}:{command} : n={len(samples)}, "
                f"{completion.average_us:.2f}/{completion.maximum_us:.2f} us, "
                f"{round_trip.average_us:.2f}/{round_trip.maximum_us:.2f} us"
            )

    grouped_sensors: dict[tuple[SensorChannel, SensorEdge], list[float]] = defaultdict(list)
    for sample in report.sensors:
        grouped_sensors[(sample.sensor, sample.edge)].append(sample.trigger_to_host_us)
    if grouped_sensors:
        lines.append("Par capteur (déclenchement->hôte moy.,max.) :")
        for (sensor, edge), values in sorted(
            grouped_sensors.items(), key=lambda item: (int(item[0][0]), item[0][1].value)
        ):
            summary = _latency_statistics(values)
            assert summary is not None
            lines.append(
                f"  C{int(sensor) + 1}:{_sensor_edge_label(edge)} : n={summary.count}, "
                f"{summary.average_us:.2f}/{summary.maximum_us:.2f} us"
            )

    lines.append(
        "Erreur d'intervalle de la référence d'horloge : "
        f"{report.reference_span_error_ppm:+.1f} ppm "
        "(comprend l'asymétrie de transport au démarrage et à l'arrêt)"
    )
    if (
        report.device_only_snapshots
        or report.unmatched_host_requests
        or report.unmatched_sensor_events
        or report.unmatched_sensor_notifications
    ):
        lines.append(
            "Non appariés : "
            f"requêtes du périphérique uniquement={report.device_only_snapshots}, "
            f"requêtes de l'hôte={report.unmatched_host_requests}, "
            f"événements des capteurs={report.unmatched_sensor_events}, "
            f"notifications des capteurs={report.unmatched_sensor_notifications}"
        )
    if report.snapshot_capacity_reached:
        lines.append(
            "AVERTISSEMENT : le tampon de 256 instantanés de requêtes du "
            "périphérique était plein"
        )
    lines.append(
        "Remarque : les valeurs unidirectionnelles hôte/périphérique sont des "
        "estimations corrélées par horloge ; le RTT est mesuré directement."
    )
    return "\n".join(lines)


format_timing_report = formatTimingReport
formaterRapportTemporisation = formatTimingReport


def plotLaunchResult(result: LaunchResult) -> None:
    """Open a combined launch overview and a four-panel detailed view."""

    if result.data is None:
        raise BoardError(
            f"le lancement ({_launch_status_label(result.status)}) n'a renvoyé "
            "aucune donnée"
        )

    import matplotlib.pyplot as plt

    data = result.data
    overview_figure, overview_axis = plt.subplots(figsize=(14, 8))
    overview_command_axis = overview_axis.twinx()
    overview_figure.canvas.manager.set_window_title("Vue d'ensemble du lancement DDA")

    detail_figure, detail_axes = plt.subplots(
        2, 2, figsize=(14, 9), sharex=True
    )
    detail_figure.canvas.manager.set_window_title("Détails du lancement DDA")
    current_axis, power_axis = detail_axes[0]
    sensor_axis, coil_axis = detail_axes[1]

    current_times = [
        index / data.sampling_frequency_hz
        for index in range(len(data.current_samples))
    ]
    bridge_colors = ("tab:blue", "tab:orange", "tab:green", "tab:red")
    for bridge_index, color in enumerate(bridge_colors):
        current_axis.plot(
            current_times,
            [
                sample[bridge_index] * _BRIDGE_CURRENT_AMPS_PER_ADC_CODE
                for sample in data.current_samples
            ],
            color=color,
            label=f"Courant H{bridge_index + 1}",
        )
        overview_axis.plot(
            current_times,
            [
                sample[bridge_index] * _BRIDGE_CURRENT_AMPS_PER_ADC_CODE
                for sample in data.current_samples
            ],
            color=color,
            label=f"Courant H{bridge_index + 1}",
        )

    labelled_edges: set[tuple[int, SensorEdge]] = set()
    for sensor_index, events in enumerate(data.sensor_events):
        for edge, timestamps, style in (
            (SensorEdge.RISING, events.rising_timestamps_ticks, "--"),
            (SensorEdge.FALLING, events.falling_timestamps_ticks, ":"),
        ):
            label_key = (sensor_index, edge)
            for timestamp in timestamps:
                elapsed_s = (
                    (timestamp - data.launch_start_ticks) & 0xFFFFFFFF
                ) / _TIM2_FREQUENCY_HZ
                overview_axis.axvline(
                    elapsed_s,
                    color=f"C{sensor_index}",
                    linestyle=style,
                    linewidth=1.2,
                    label=(
                        f"S{sensor_index + 1} {_sensor_edge_label(edge)}"
                        if label_key not in labelled_edges
                        else None
                    ),
                )
                sensor_axis.scatter(
                    elapsed_s,
                    sensor_index,
                    color=f"C{sensor_index}",
                    marker="^" if edge is SensorEdge.RISING else "v",
                    label=(
                        f"S{sensor_index + 1} {_sensor_edge_label(edge)}"
                        if label_key not in labelled_edges
                        else None
                    ),
                )
                labelled_edges.add(label_key)

    power_frequency_hz = data.sampling_frequency_hz / 10.0
    power_axis.plot(
        [index / power_frequency_hz for index in range(len(data.power_samples))],
        [sample * _POWER_LSB_W for sample in data.power_samples],
        color="tab:purple",
        label="Puissance de la carte",
    )

    command_styles = {
        int(_CoilCommand.FORWARD): ("direct", "^"),
        int(_CoilCommand.REVERSE): ("inverse", "v"),
        int(_CoilCommand.COIL_OFF): ("arrêt", "x"),
    }
    labelled_commands: set[tuple[int, int]] = set()
    for event in data.request_snapshots:
        if event.service is not Service.COILS or event.command in (
            int(_CoilCommand.WAKE),
            int(_CoilCommand.SLEEP),
            int(_CoilCommand.SET_CURRENT),
        ):
            continue
        elapsed_s = (
            (event.execution_timestamp_ticks - data.launch_start_ticks)
            & 0xFFFFFFFF
        ) / _TIM2_FREQUENCY_HZ
        style = command_styles.get(event.command)
        if style is not None:
            name, marker = style
            bridges = (
                range(4)
                if event.options == int(Bridge.ALL)
                else (event.options,)
            )
            for bridge in bridges:
                if 0 <= bridge < 4:
                    label_key = (event.command, bridge)
                    label = (
                        f"H{bridge + 1} {name}"
                        if label_key not in labelled_commands
                        else None
                    )
                    coil_axis.scatter(
                        elapsed_s,
                        bridge,
                        marker=marker,
                        color=bridge_colors[bridge],
                        label=label,
                    )
                    overview_command_axis.scatter(
                        elapsed_s,
                        bridge,
                        marker=marker,
                        color=bridge_colors[bridge],
                        label=label,
                    )
                    labelled_commands.add(label_key)
        elif event.command == int(_CoilCommand.GET_FAULTS):
            label_key = (event.command, -1)
            command_label = (
                "Lecture des défauts"
                if label_key not in labelled_commands
                else None
            )
            coil_axis.axvline(
                elapsed_s,
                color="tab:gray",
                linestyle=":",
                label=command_label,
            )
            overview_command_axis.axvline(
                elapsed_s,
                color="tab:gray",
                linestyle=":",
                label=command_label,
            )
            coil_axis.text(
                elapsed_s,
                3.4,
                "Lecture des défauts",
                rotation=90,
                va="bottom",
                ha="right",
                fontsize=8,
            )
            overview_command_axis.text(
                elapsed_s,
                3.4,
                "Lecture des défauts",
                rotation=90,
                va="bottom",
                ha="right",
                fontsize=8,
            )
            labelled_commands.add(label_key)

    overview_axis.set_title(
        "Vue d'ensemble du lancement - "
        f"{_launch_status_label(result.status)} : courants, capteurs et bobines"
    )
    overview_axis.set_ylabel("Courant des ponts en H (A)")
    overview_axis.set_xlabel("Temps depuis le début du lancement (s)")
    overview_axis.grid(True, alpha=0.3)
    overview_command_axis.set_ylabel("Pont de bobine commandé")
    overview_command_axis.set_yticks(range(4))
    overview_command_axis.set_yticklabels(("H1", "H2", "H3", "H4"))
    overview_command_axis.set_ylim(-0.5, 3.8)

    overview_handles, overview_labels = overview_axis.get_legend_handles_labels()
    command_handles, command_labels = (
        overview_command_axis.get_legend_handles_labels()
    )
    legend_entries = dict(
        zip(
            overview_labels + command_labels,
            overview_handles + command_handles,
        )
    )
    if legend_entries:
        overview_axis.legend(
            legend_entries.values(),
            legend_entries.keys(),
            loc="upper center",
            bbox_to_anchor=(0.5, -0.1),
            ncol=4,
        )
    overview_figure.tight_layout()

    current_axis.set_title("Courant des ponts en H")
    current_axis.set_ylabel("Courant (A)")
    current_axis.legend(loc="best")
    current_axis.grid(True, alpha=0.3)
    power_axis.set_title(
        "Puissance de la carte - "
        f"{data.missed_power_samples} échantillon(s) manqué(s)"
    )
    power_axis.set_ylabel("Puissance (W)")
    power_axis.legend(loc="best")
    power_axis.grid(True, alpha=0.3)

    sensor_axis.set_title("Déclenchements des capteurs")
    sensor_axis.set_yticks(range(4))
    sensor_axis.set_yticklabels(("S1", "S2", "S3", "S4"))
    sensor_axis.set_ylim(-0.5, 3.5)
    sensor_axis.set_xlabel("Temps depuis le début du lancement (s)")
    sensor_axis.grid(True, alpha=0.3)
    if labelled_edges:
        sensor_axis.legend(loc="best")

    coil_axis.set_title("Commandes des bobines")
    coil_axis.set_yticks(range(4))
    coil_axis.set_yticklabels(("H1", "H2", "H3", "H4"))
    coil_axis.set_ylim(-0.5, 3.8)
    coil_axis.set_xlabel("Temps depuis le début du lancement (s)")
    coil_axis.grid(True, alpha=0.3)
    if labelled_commands:
        coil_axis.legend(loc="best")
    detail_figure.tight_layout()

    report = result.timing_report
    if report is not None and (report.requests or report.sensors):
        latency_figure, latency_axes = plt.subplots(2, 1, figsize=(12, 8))
        latency_figure.canvas.manager.set_window_title("Latence de débogage DDA")
        request_axis, notification_axis = latency_axes
        if report.requests:
            request_indices = range(1, len(report.requests) + 1)
            request_axis.plot(
                request_indices,
                [sample.host_to_completion_us for sample in report.requests],
                marker="o",
                label="Hôte vers fin du traitement par le périphérique (estimée)",
            )
            request_axis.plot(
                request_indices,
                [sample.round_trip_us for sample in report.requests],
                marker="x",
                label="Aller-retour de l'hôte",
            )
            request_axis.legend(loc="best")
        request_axis.set_title("Latence des requêtes")
        request_axis.set_xlabel("Numéro de la requête appariée")
        request_axis.set_ylabel("Latence (µs)")
        request_axis.grid(True, alpha=0.3)

        if report.sensors:
            sensor_indices = range(1, len(report.sensors) + 1)
            notification_axis.plot(
                sensor_indices,
                [sample.trigger_to_host_us for sample in report.sensors],
                marker="o",
                color="tab:green",
                label="Déclenchement du capteur vers l'hôte (estimée)",
            )
            notification_axis.legend(loc="best")
        notification_axis.set_title("Latence des notifications des capteurs")
        notification_axis.set_xlabel("Numéro de la notification appariée")
        notification_axis.set_ylabel("Latence (µs)")
        notification_axis.grid(True, alpha=0.3)
        latency_figure.tight_layout()
    plt.show()


plot_launch_result = plotLaunchResult
tracerResultatLancement = plotLaunchResult


class _SerialConnection(Protocol):
    timeout: Optional[float]

    def write(self, data: bytes) -> int: ...

    def read(self, size: int = 1) -> bytes: ...

    def close(self) -> None: ...

    def reset_input_buffer(self) -> None: ...


@dataclass(slots=True)
class _PendingRequest:
    service: Service
    command: int
    options: int
    event: threading.Event = field(default_factory=threading.Event)
    response: Optional[int] = None
    sent_at_ns: int = 0


@dataclass(frozen=True, slots=True)
class _HostRequestTiming:
    service: Service
    command: int
    options: int
    sent_at_ns: int
    received_at_ns: int


@dataclass(slots=True)
class _LaunchTimingCapture:
    """Mutable host timestamps retained only for one debug launch."""

    active: bool = False
    requests: list[_HostRequestTiming] = field(default_factory=list)
    sensors: list[SensorNotification] = field(default_factory=list)
    start_reference_ns: int = 0
    end_reference_ns: int = 0

    def begin(self, enabled: bool) -> None:
        self.active = enabled
        self.requests.clear()
        self.sensors.clear()
        self.start_reference_ns = 0
        self.end_reference_ns = 0

    def finish(self) -> None:
        self.active = False

    def has_clock_references(self) -> bool:
        return self.start_reference_ns > 0 and self.end_reference_ns > 0


def _tick_delta(start: int, end: int) -> int:
    return (end - start) & 0xFFFFFFFF


def _build_timing_report(
    data: LaunchData,
    host_requests: tuple[_HostRequestTiming, ...],
    sensor_notifications: tuple[SensorNotification, ...],
    host_start_ns: int,
    host_end_ns: int,
) -> LaunchTimingReport:
    device_span_ticks = _tick_delta(data.launch_start_ticks, data.launch_end_ticks)
    host_span_ns = host_end_ns - host_start_ns
    if device_span_ticks == 0 or host_span_ns <= 0:
        raise ValueError("invalid launch timing reference span")

    def device_to_host_ns(timestamp_ticks: int) -> float:
        elapsed_ticks = _tick_delta(data.launch_start_ticks, timestamp_ticks)
        return host_start_ns + elapsed_ticks * host_span_ns / device_span_ticks

    snapshots_by_key: dict[tuple[Service, int, int], deque[RequestSnapshot]] = (
        defaultdict(deque)
    )
    for snapshot in data.request_snapshots:
        snapshots_by_key[(snapshot.service, snapshot.command, snapshot.options)].append(
            snapshot
        )

    request_samples: list[RequestLatency] = []
    unmatched_host_requests = 0
    for host_request in host_requests:
        if (
            host_request.service is Service.LAUNCH
            and host_request.command == int(_LaunchCommand.STOP)
        ):
            # LaunchData freezes its snapshot count before STOP completes.
            continue
        snapshots = snapshots_by_key[
            (host_request.service, host_request.command, host_request.options)
        ]
        if not snapshots:
            unmatched_host_requests += 1
            continue
        snapshot = snapshots.popleft()
        completion_ticks = snapshot.execution_timestamp_ticks
        completion_host_ns = device_to_host_ns(completion_ticks)
        done_host_ns = device_to_host_ns(snapshot.done_at_ticks)
        device_processing_ticks = _tick_delta(
            snapshot.created_at_ticks, completion_ticks
        )
        completion_to_queue_ticks = _tick_delta(
            completion_ticks, snapshot.done_at_ticks
        )
        request_samples.append(
            RequestLatency(
                host_request.service,
                host_request.command,
                host_request.options,
                (completion_host_ns - host_request.sent_at_ns) / 1_000.0,
                (host_request.received_at_ns - host_request.sent_at_ns) / 1_000.0,
                device_processing_ticks / 64.0,
                completion_to_queue_ticks / 64.0,
                _tick_delta(
                    snapshot.created_at_ticks, snapshot.done_at_ticks
                )
                / 64.0,
                (host_request.received_at_ns - done_host_ns) / 1_000.0,
            )
        )

    device_only_snapshots = sum(len(snapshots) for snapshots in snapshots_by_key.values())

    device_sensor_events: dict[
        tuple[SensorChannel, SensorEdge], deque[int]
    ] = defaultdict(deque)
    for sensor_index, events in enumerate(data.sensor_events):
        sensor = SensorChannel(sensor_index)
        device_sensor_events[(sensor, SensorEdge.RISING)].extend(
            events.rising_timestamps_ticks
        )
        device_sensor_events[(sensor, SensorEdge.FALLING)].extend(
            events.falling_timestamps_ticks
        )

    sensor_samples: list[SensorLatency] = []
    unmatched_sensor_notifications = 0
    for notification in sensor_notifications:
        events = device_sensor_events[(notification.sensor, notification.edge)]
        if not events:
            unmatched_sensor_notifications += 1
            continue
        trigger_ticks = events.popleft()
        sensor_samples.append(
            SensorLatency(
                notification.sensor,
                notification.edge,
                notification.received_at_s * 1_000_000.0
                - device_to_host_ns(trigger_ticks) / 1_000.0,
            )
        )
    unmatched_sensor_events = sum(
        len(events) for events in device_sensor_events.values()
    )
    device_span_ns = device_span_ticks * 1_000_000_000.0 / _TIM2_FREQUENCY_HZ
    return LaunchTimingReport(
        tuple(request_samples),
        tuple(sensor_samples),
        device_only_snapshots,
        unmatched_host_requests,
        unmatched_sensor_events,
        unmatched_sensor_notifications,
        len(data.request_snapshots) == _SNAPSHOT_CAPACITY,
        (host_span_ns / device_span_ns - 1.0) * 1_000_000.0,
    )


class _ReceiverStopped(Exception):
    pass


class CompetitionBoard:
    """Easy participant API with one hidden serial-receive worker.

    Participant methods run on the main thread. The one background thread is
    the only serial reader and handles replies, faults, sensor notifications,
    run status, LaunchData assembly, and CRC validation.

    Competition-test programs normally select ``DeviceMode.DEBUG`` so coil
    commands remain available during a run. ``DeviceMode.COMPETITION`` exposes
    the real firmware lockout for testing as well.
    """

    def __init__(
        self,
        connection: _SerialConnection,
        *,
        response_timeout_s: float = 1.0,
        launch_timeout_s: float = 10.0,
    ) -> None:
        if response_timeout_s <= 0 or launch_timeout_s <= 0:
            raise ValueError("timeouts must be greater than zero")
        self._serial = connection
        self._response_timeout_s = response_timeout_s
        self._launch_timeout_s = launch_timeout_s
        self._request_lock = threading.Lock()
        self._state_lock = threading.RLock()
        self._sensor_condition = threading.Condition(self._state_lock)
        self._stop_event = threading.Event()
        self._launch_status_event = threading.Event()
        self._launch_data_event = threading.Event()
        self._pending: Optional[_PendingRequest] = None
        self._last_status: dict[tuple[Service, int], int] = {}
        self._sensor_notifications: list[SensorNotification] = []
        self._faults = PowerStageFault(0)
        self._mode = DeviceMode.DEBUG
        self._sampling_frequency_hz = 5000
        self._launch_active = False
        self._active_run_id: Optional[int] = None
        self._next_run_id = 0
        self._launch_status: Optional[LaunchStatus] = None
        self._launch_data: Optional[LaunchData] = None
        self._launch_data_error: Optional[Exception] = None
        self._launch_rx_run_id: Optional[int] = None
        self._launch_rx_chunk = 0
        self._launch_rx_payload = bytearray()
        self._timing = _LaunchTimingCapture()
        self._background_error: Optional[Exception] = None
        self._closed = False
        self._receiver_generation = 0
        self._receiver = self._start_receiver()

    @classmethod
    def connect(
        cls,
        port: Optional[str] = None,
        *,
        mode: DeviceMode = DeviceMode.DEBUG,
        response_timeout_s: float = 1.0,
        launch_timeout_s: float = 10.0,
    ) -> "CompetitionBoard":
        """Open a board and select Debug or normal Competition mode."""

        try:
            import serial
            from serial.tools import list_ports
        except ImportError as error:
            raise RuntimeError("pyserial is required: pip install pyserial") from error

        matching_ports = [
            item
            for item in list_ports.comports()
            if item.vid == STM32_VID and item.pid == STM32_CDC_PID
        ]
        if port is None:
            if not matching_ports:
                raise BoardError("no STM32 Virtual ComPort was found")
            if len(matching_ports) != 1:
                raise BoardError("multiple boards found; pass port='COMx'")
            selected_port = matching_ports[0].device
        else:
            selected_port = port

        connection = serial.Serial(
            selected_port,
            baudrate=115200,
            timeout=0.05,
            write_timeout=response_timeout_s,
        )
        time.sleep(0.1)
        connection.reset_input_buffer()
        board = cls(
            connection,
            response_timeout_s=response_timeout_s,
            launch_timeout_s=launch_timeout_s,
        )
        try:
            board.setMode(mode)
        except BaseException:
            board.close()
            raise
        return board

    @property
    def mode(self) -> DeviceMode:
        with self._state_lock:
            return self._mode

    @property
    def faults(self) -> PowerStageFault:
        with self._state_lock:
            return self._faults

    @property
    def launch_status(self) -> Optional[LaunchStatus]:
        with self._state_lock:
            return self._launch_status

    @property
    def launch_active(self) -> bool:
        with self._state_lock:
            return self._launch_active

    @property
    def background_error(self) -> Optional[Exception]:
        with self._state_lock:
            return self._background_error

    def setMode(self, mode: DeviceMode | int) -> None:
        selected = DeviceMode(mode)
        self._expect_success(
            Service.LAUNCH,
            _LaunchCommand.SET_DEBUG_MODE,
            int(selected),
            "set device mode",
        )
        with self._state_lock:
            self._mode = selected

    def setSamplingFrequency(self, frequency_hz: int) -> None:
        if frequency_hz % 100 or not 100 <= frequency_hz <= 5000:
            raise ValueError("frequency_hz must be 100..5000 in 100 Hz steps")
        self._expect_success(
            Service.LAUNCH,
            _LaunchCommand.SET_SAMPLING_RATE,
            frequency_hz // 100,
            "set sampling frequency",
        )
        with self._state_lock:
            self._sampling_frequency_hz = frequency_hz

    def startLaunch(self, run_id: Optional[int] = None) -> int:
        with self._state_lock:
            if self._launch_active:
                raise BoardError("a launch is already active")
            selected_run_id = self._next_run_id if run_id is None else run_id
            if not 0 <= selected_run_id <= _OPTION_MASK:
                raise ValueError("run_id must be between 0 and 127")
            self._next_run_id = (selected_run_id + 1) & _OPTION_MASK
            self._active_run_id = selected_run_id
            self._launch_status = None
            self._launch_data = None
            self._launch_data_error = None
            self._launch_status_event.clear()
            self._launch_data_event.clear()
            self._reset_launch_receiver()
            self._timing.begin(self._mode is DeviceMode.DEBUG)

        try:
            self._expect_success(
                Service.LAUNCH,
                _LaunchCommand.START,
                selected_run_id,
                "start launch",
            )
        except Exception:
            with self._state_lock:
                self._active_run_id = None
                self._timing.finish()
            raise
        with self._state_lock:
            self._launch_active = True
        return selected_run_id

    def stopLaunch(self, timeout_s: Optional[float] = None) -> LaunchResult:
        timeout = self._launch_timeout_s if timeout_s is None else timeout_s
        if timeout <= 0:
            raise ValueError("timeout_s must be greater than zero")

        with self._state_lock:
            if self._active_run_id is None:
                raise BoardError("no launch has been started")
            final_status_already_received = self._launch_status is not None

        if not final_status_already_received:
            try:
                command_status = self._request(
                    Service.LAUNCH, _LaunchCommand.STOP
                )
            except BoardError:
                with self._state_lock:
                    final_status_already_received = self._launch_status is not None
                if not final_status_already_received:
                    raise
            else:
                if command_status != int(LaunchStatus.SUCCESS):
                    with self._state_lock:
                        final_status_already_received = (
                            self._launch_status is not None
                        )
                    if not final_status_already_received:
                        self._raise_rejected("stop launch", command_status)

        status = self._wait_for_launch_status(timeout)
        data = None
        if status is not LaunchStatus.HOST_ABORTED:
            data = self._wait_for_launch_data(timeout)
        timing_report = None
        with self._state_lock:
            if (
                data is not None
                and self._timing.active
                and self._timing.has_clock_references()
            ):
                try:
                    timing_report = _build_timing_report(
                        data,
                        tuple(self._timing.requests),
                        tuple(self._timing.sensors),
                        self._timing.start_reference_ns,
                        self._timing.end_reference_ns,
                    )
                except ValueError:
                    # Diagnostics must never invalidate otherwise sound launch
                    # data when a run is too short to establish clock anchors.
                    timing_report = None
            self._active_run_id = None
            self._launch_active = False
            self._timing.finish()
        return LaunchResult(status, data, timing_report)

    def abortLaunch(self, timeout_s: Optional[float] = None) -> LaunchResult:
        timeout = self._launch_timeout_s if timeout_s is None else timeout_s
        with self._state_lock:
            if self._active_run_id is None:
                raise BoardError("no launch has been started")
        status = self._request(Service.LAUNCH, _LaunchCommand.ABORT)
        if status != int(LaunchStatus.SUCCESS):
            self._raise_rejected("abort launch", status)
        final_status = self._wait_for_launch_status(timeout)
        with self._state_lock:
            self._active_run_id = None
            self._launch_active = False
            self._timing.finish()
        return LaunchResult(final_status, None)

    def drive(
        self,
        direction: Direction | str,
        bridge: Bridge | int = Bridge.ALL,
    ) -> None:
        selected_direction = Direction(direction)
        command = (
            _CoilCommand.FORWARD
            if selected_direction is Direction.FORWARD
            else _CoilCommand.REVERSE
        )
        self._expect_success(
            Service.COILS, command, int(Bridge(bridge)), "drive bridge"
        )

    def forward(self, bridge: Bridge | int = Bridge.ALL) -> None:
        self.drive(Direction.FORWARD, bridge)

    def reverse(self, bridge: Bridge | int = Bridge.ALL) -> None:
        self.drive(Direction.REVERSE, bridge)

    def wake(self, bridge: Bridge | int = Bridge.ALL) -> None:
        self._expect_success(
            Service.COILS,
            _CoilCommand.WAKE,
            int(Bridge(bridge)),
            "wake bridge",
        )

    def sleep(self, bridge: Bridge | int = Bridge.ALL) -> None:
        self._expect_success(
            Service.COILS,
            _CoilCommand.SLEEP,
            int(Bridge(bridge)),
            "sleep bridge",
        )

    def off(self, bridge: Bridge | int = Bridge.ALL) -> None:
        """Stop bridge drive while leaving the selected driver(s) awake."""

        self._expect_success(
            Service.COILS,
            _CoilCommand.COIL_OFF,
            int(Bridge(bridge)),
            "turn bridge off",
        )

    coilOff = off
    coil_off = off

    def setCurrent(
        self,
        current_ma: int,
        bridge: Bridge | int = Bridge.ALL,
    ) -> None:
        if not 0 <= current_ma <= 3000 or current_ma % _CURRENT_STEP_MA:
            raise ValueError("current_ma must be 0..3000 in 25 mA steps")
        selected_bridge = Bridge(bridge)
        command = _CoilCommand.SET_CURRENT
        if selected_bridge is not Bridge.ALL:
            command = _CoilCommand(
                int(_CoilCommand.SET_CURRENT_H1) + int(selected_bridge)
            )
        self._expect_success(
            Service.COILS,
            command,
            current_ma // _CURRENT_STEP_MA,
            "set coil current",
        )

    def getCurrent(self, bridge: Bridge | int) -> int:
        selected_bridge = Bridge(bridge)
        if selected_bridge is Bridge.ALL:
            raise ValueError("getCurrent requires one bridge (H1..H4)")
        command = _CoilCommand(
            int(_CoilCommand.GET_CURRENT_H1) + int(selected_bridge)
        )
        current_units = self._request(Service.COILS, command)
        if current_units > 3000 // _CURRENT_STEP_MA:
            raise BoardProtocolError("invalid current-threshold response")
        return current_units * _CURRENT_STEP_MA

    def setPmode(self, pwm_mode: bool) -> None:
        if not isinstance(pwm_mode, bool):
            raise TypeError("pwm_mode must be a bool")
        self._expect_success(
            Service.COILS,
            _CoilCommand.SET_PMODE,
            int(pwm_mode),
            "set PMODE",
        )

    def getPmode(self) -> bool:
        response = self._request(Service.COILS, _CoilCommand.GET_PMODE)
        if response not in (0, 1):
            raise BoardProtocolError("invalid PMODE response")
        return bool(response)

    def getFaults(self) -> PowerStageFault:
        faults = PowerStageFault(
            self._request(Service.SAFETY, _SafetyCommand.GET_FAULTS)
        )
        with self._state_lock:
            self._faults = faults
        return faults

    def getSystemState(self) -> SystemState:
        return SystemState(
            self._request(Service.SAFETY, _SafetyCommand.GET_STATE)
        )

    def startSensorCalibration(
        self, timeout_s: float = _CALIBRATION_TIMEOUT_S
    ) -> None:
        """Calibrate all sensors and wait for the board to save the values."""

        status = self._request(
            Service.SENSORS,
            _SensorCommand.CALIBRATE,
            timeout_s=timeout_s,
        )
        if status != 0:
            self._raise_rejected("calibrate sensors", status)

    def readSensorCalibration(
        self, sensor: SensorChannel | int
    ) -> SensorCalibration:
        """Read the two stored DAC codes for one sensor."""

        selected = SensorChannel(sensor)
        if selected is SensorChannel.ALL:
            raise ValueError("readSensorCalibration requires one sensor")
        led_code = self._request(
            Service.SENSORS,
            _SensorCommand.READ_CALIBRATION_LED_CODE,
            int(selected),
        )
        trip_code = self._request(
            Service.SENSORS,
            _SensorCommand.READ_CALIBRATION_TRIP_CODE,
            int(selected),
        )
        if _CALIBRATION_VALUE_UNAVAILABLE in (led_code, trip_code):
            raise BoardError(f"no stored calibration for {selected.name}")
        return SensorCalibration(selected, led_code, trip_code)

    def calibrateSensors(
        self, timeout_s: float = _CALIBRATION_TIMEOUT_S
    ) -> tuple[SensorCalibration, ...]:
        """Calibrate all four sensors and return their saved DAC codes."""

        self.startSensorCalibration(timeout_s)
        return tuple(
            self.readSensorCalibration(sensor)
            for sensor in (
                SensorChannel.SENSOR_1,
                SensorChannel.SENSOR_2,
                SensorChannel.SENSOR_3,
                SensorChannel.SENSOR_4,
            )
        )

    def useDefaultSensorLevels(
        self, sensor: SensorChannel | int = SensorChannel.ALL
    ) -> None:
        self._expect_success(
            Service.SENSORS,
            _SensorCommand.SET_DEFAULT_LEVELS,
            int(SensorChannel(sensor)),
            "set default sensor levels",
        )

    def useCalibratedSensorLevels(
        self, sensor: SensorChannel | int = SensorChannel.ALL
    ) -> None:
        self._expect_success(
            Service.SENSORS,
            _SensorCommand.SET_CALIBRATED_LEVELS,
            int(SensorChannel(sensor)),
            "set calibrated sensor levels",
        )

    def unlockSensor(self) -> None:
        self._expect_success(
            Service.SENSORS,
            _SensorCommand.UNLOCK_SENSOR,
            0,
            "unlock sensor notifications",
        )

    def waitForSensor(
        self,
        sensor: SensorChannel | int = SensorChannel.ALL,
        edge: Optional[SensorEdge | str] = None,
        timeout_s: Optional[float] = None,
    ) -> SensorNotification:
        selected_sensor = SensorChannel(sensor)
        selected_edge = None if edge is None else SensorEdge(edge)
        deadline = None if timeout_s is None else time.monotonic() + timeout_s
        if timeout_s is not None and timeout_s <= 0:
            raise ValueError("timeout_s must be greater than zero")

        with self._sensor_condition:
            launch_run_id = self._active_run_id
            while True:
                for index, notification in enumerate(self._sensor_notifications):
                    if (
                        selected_sensor in (SensorChannel.ALL, notification.sensor)
                        and selected_edge in (None, notification.edge)
                    ):
                        return self._sensor_notifications.pop(index)
                self._raise_background_error_locked()
                if launch_run_id is not None and self._launch_status is not None:
                    raise BoardTimeoutError(
                        "launch ended with status "
                        f"{self._launch_status.name} while waiting for a sensor event"
                    )
                remaining = (
                    None if deadline is None else deadline - time.monotonic()
                )
                if remaining is not None and remaining <= 0:
                    raise BoardTimeoutError("timed out waiting for a sensor event")
                self._sensor_condition.wait(remaining)

    def takeSensorNotifications(self) -> tuple[SensorNotification, ...]:
        with self._sensor_condition:
            notifications = tuple(self._sensor_notifications)
            self._sensor_notifications.clear()
            return notifications

    # API en francais destinee au code des participants.
    @classmethod
    def connecter(
        cls,
        port: Optional[str] = None,
        *,
        mode: ModeCarte = ModeCarte.DEBUG,
        delai_reponse_s: float = 1.0,
        delai_lancement_s: float = 10.0,
    ) -> "CarteCompetition":
        """Ouvre la connexion avec la carte, avec detection automatique du port."""

        return cls.connect(
            port,
            mode=mode,
            response_timeout_s=delai_reponse_s,
            launch_timeout_s=delai_lancement_s,
        )

    @property
    def defauts(self) -> DefautEtagePuissance:
        """Retourne les derniers defauts signales par la carte."""

        return self.faults

    @property
    def etatLancement(self) -> Optional[EtatLancement]:
        """Retourne l'etat final du dernier lancement, s'il est disponible."""

        return self.launch_status

    @property
    def lancementActif(self) -> bool:
        """Indique si un lancement est actuellement actif."""

        return self.launch_active

    @property
    def erreurArrierePlan(self) -> Optional[Exception]:
        """Retourne l'erreur de communication asynchrone courante."""

        return self.background_error

    def reglerMode(self, mode: ModeCarte | int) -> None:
        """Selectionne le mode de fonctionnement de la carte."""

        self.setMode(mode)

    def reglerFrequenceEchantillonnage(self, frequence_hz: int) -> None:
        """Regle la frequence d'echantillonnage entre 100 et 5000 Hz."""

        self.setSamplingFrequency(frequence_hz)

    def demarrerLancement(self, identifiant: Optional[int] = None) -> int:
        """Demarre une tentative de lancement."""

        return self.startLaunch(identifiant)

    def arreterLancement(
        self, delai_s: Optional[float] = None
    ) -> ResultatLancement:
        """Arrete le lancement et retourne les donnees mesurees."""

        return self.stopLaunch(delai_s)

    def annulerLancement(
        self, delai_s: Optional[float] = None
    ) -> ResultatLancement:
        """Annule immediatement le lancement actif."""

        return self.abortLaunch(delai_s)

    def reglerCourant(
        self,
        courant_ma: int,
        bobine: Bobine | int = Bobine.TOUTES,
    ) -> None:
        """Regle le courant des bobines selectionnees en milliamperes."""

        self.setCurrent(courant_ma, bobine)

    def lireCourant(self, bobine: Bobine | int) -> int:
        """Lit le courant demande pour une bobine en milliamperes."""

        return self.getCurrent(bobine)

    def reglerPmode(self, mode_pwm: bool) -> None:
        """Selectionne le mode PWM (vrai) ou PH/EN (faux)."""

        self.setPmode(mode_pwm)

    def lirePmode(self) -> bool:
        """Retourne vrai lorsque les pilotes sont en mode PWM."""

        return self.getPmode()

    def activer(
        self,
        bobine: Bobine | int,
        direction: Direction | str,
    ) -> None:
        """Alimente la bobine selectionnee dans la direction demandee."""

        self.drive(direction, bobine)

    def desactiver(self, bobine: Bobine | int = Bobine.TOUTES) -> None:
        """Coupe la bobine selectionnee sans mettre son pilote en veille."""

        self.off(bobine)

    def reveiller(self, bobine: Bobine | int = Bobine.TOUTES) -> None:
        """Reveille le pilote de la bobine selectionnee."""

        self.wake(bobine)

    def mettreEnVeille(self, bobine: Bobine | int = Bobine.TOUTES) -> None:
        """Met en veille le pilote de la bobine selectionnee."""

        self.sleep(bobine)

    def attendreCapteur(
        self,
        capteur: Capteur | int = Capteur.TOUS,
        front: Optional[FrontCapteur | str] = None,
        delai_s: Optional[float] = None,
    ) -> NotificationCapteur:
        """Attend le prochain signal du capteur selectionne."""

        return self.waitForSensor(capteur, front, delai_s)

    def prendreNotificationsCapteurs(
        self,
    ) -> tuple[NotificationCapteur, ...]:
        """Retourne et efface les notifications de capteurs en attente."""

        return self.takeSensorNotifications()

    def lireDefauts(self) -> DefautEtagePuissance:
        """Lit et retourne les defauts de l'etage de puissance."""

        return self.getFaults()

    def lireEtatSysteme(self) -> EtatSysteme:
        """Lit et retourne l'etat de fonctionnement de la carte."""

        return self.getSystemState()

    def demarrerEtalonnageCapteurs(self, delai_s: float = 10.0) -> None:
        """Etalonne les quatre capteurs et attend la sauvegarde des valeurs."""

        self.startSensorCalibration(delai_s)

    def lireEtalonnageCapteur(
        self, capteur: Capteur | int
    ) -> EtalonnageCapteur:
        """Lit les valeurs d'etalonnage sauvegardees pour un capteur."""

        return self.readSensorCalibration(capteur)

    def etalonnerCapteurs(
        self, delai_s: float = 10.0
    ) -> tuple[EtalonnageCapteur, ...]:
        """Etalonne tous les capteurs et retourne leurs valeurs sauvegardees."""

        return self.calibrateSensors(delai_s)

    def utiliserNiveauxCapteursParDefaut(
        self, capteur: Capteur | int = Capteur.TOUS
    ) -> None:
        """Applique les seuils par defaut aux capteurs selectionnes."""

        self.useDefaultSensorLevels(capteur)

    def utiliserNiveauxCapteursEtalonnes(
        self, capteur: Capteur | int = Capteur.TOUS
    ) -> None:
        """Applique les seuils etalonnes aux capteurs selectionnes."""

        self.useCalibratedSensorLevels(capteur)

    def deverrouillerCapteurs(self) -> None:
        """Rearme les notifications des capteurs."""

        self.unlockSensor()

    def fermer(self) -> None:
        """Ferme la connexion en replaçant la carte dans un etat securitaire."""

        self.close()

    # Python-style aliases are provided without duplicating behavior.
    set_mode = setMode
    set_sampling_frequency = setSamplingFrequency
    start_launch = startLaunch
    stop_launch = stopLaunch
    abort_launch = abortLaunch
    set_current = setCurrent
    get_current = getCurrent
    set_pmode = setPmode
    get_pmode = getPmode
    get_faults = getFaults
    get_system_state = getSystemState
    start_sensor_calibration = startSensorCalibration
    read_sensor_calibration = readSensorCalibration
    calibrate_sensors = calibrateSensors
    use_default_sensor_levels = useDefaultSensorLevels
    use_calibrated_sensor_levels = useCalibratedSensorLevels
    unlock_sensor = unlockSensor
    wait_for_sensor = waitForSensor
    take_sensor_notifications = takeSensorNotifications

    def _expect_success(
        self, service: Service, command: IntEnum, options: int, operation: str
    ) -> None:
        status = self._request(service, command, options)
        if status != 0:
            self._raise_rejected(operation, status)

    @staticmethod
    def _raise_rejected(operation: str, status: int) -> None:
        raise BoardRejectedError(f"{operation} rejected with status {status}")

    def _request(
        self,
        service: Service,
        command: IntEnum,
        options: int = 0,
        timeout_s: Optional[float] = None,
    ) -> int:
        if not 0 <= int(options) <= _OPTION_MASK:
            raise ValueError("request options must be between 0 and 127")
        timeout = self._response_timeout_s if timeout_s is None else timeout_s
        if timeout <= 0:
            raise ValueError("timeout_s must be greater than zero")

        with self._request_lock:
            self._ensure_open()
            pending = _PendingRequest(service, int(command), int(options))
            with self._state_lock:
                self._raise_background_error_locked()
                self._pending = pending
            frame = bytes((int(service), int(command), int(options) | _ANSWER_BIT))
            try:
                pending.sent_at_ns = time.perf_counter_ns()
                with self._state_lock:
                    if self._timing.active:
                        if (
                            service is Service.LAUNCH
                            and int(command) == int(_LaunchCommand.START)
                        ):
                            self._timing.start_reference_ns = pending.sent_at_ns
                        elif (
                            service is Service.LAUNCH
                            and int(command) == int(_LaunchCommand.STOP)
                        ):
                            self._timing.end_reference_ns = pending.sent_at_ns
                if self._serial.write(frame) != len(frame):
                    raise BoardTimeoutError("serial write did not complete")
                if not pending.event.wait(timeout):
                    raise BoardTimeoutError(
                        f"no response from {service.name} command {int(command)}"
                    )
                with self._state_lock:
                    if pending.response is None:
                        self._raise_background_error_locked()
                        raise BoardProtocolError(
                            "request completed without a response"
                        )
                    return pending.response
            finally:
                # BaseException (notably Ctrl+C) must not leave a stale request
                # behind while the board is being aborted or the port is closed.
                with self._state_lock:
                    if self._pending is pending:
                        self._pending = None

    def _start_receiver(self) -> threading.Thread:
        self._receiver_generation += 1
        generation = self._receiver_generation
        receiver = threading.Thread(
            target=self._receive_loop,
            args=(generation,),
            name="DDA USB receiver",
            daemon=True,
        )
        receiver.start()
        return receiver

    def _receive_loop(self, generation: int) -> None:
        connection = self._serial
        try:
            while (
                not self._stop_event.is_set()
                and generation == self._receiver_generation
            ):
                first = connection.read(1)
                if not first:
                    continue
                if first[0] == _LAUNCH_MARKER:
                    header = first + self._read_exact_background(
                        connection,
                        _LAUNCH_HEADER_SIZE - 1,
                        generation,
                    )
                    payload = self._read_exact_background(
                        connection, header[6], generation
                    )
                    self._handle_launch_frame(header, payload)
                else:
                    packet = first + self._read_exact_background(
                        connection, 2, generation
                    )
                    self._handle_packet(
                        packet[0], packet[1], packet[2], time.perf_counter_ns()
                    )
        except _ReceiverStopped:
            pass
        except Exception as error:
            if (
                not self._stop_event.is_set()
                and generation == self._receiver_generation
            ):
                self._fail_background(error)

    def _read_exact_background(
        self,
        connection: _SerialConnection,
        length: int,
        generation: int,
    ) -> bytes:
        data = bytearray()
        deadline = time.monotonic() + _USB_FRAME_TIMEOUT_S
        while len(data) < length:
            if (
                self._stop_event.is_set()
                or generation != self._receiver_generation
            ):
                raise _ReceiverStopped
            chunk = connection.read(length - len(data))
            if chunk:
                data.extend(chunk)
                deadline = time.monotonic() + _USB_FRAME_TIMEOUT_S
            elif time.monotonic() >= deadline:
                raise BoardTimeoutError("USB frame reception timed out")
        return bytes(data)

    def _handle_packet(
        self,
        source_value: int,
        command: int,
        options: int,
        received_at_ns: Optional[int] = None,
    ) -> None:
        if received_at_ns is None:
            received_at_ns = time.perf_counter_ns()
        try:
            source = Service(source_value)
        except ValueError as error:
            raise BoardProtocolError(
                f"invalid response service {source_value}"
            ) from error

        if source is Service.SENSORS and command == _SensorCommand.REPORT_SENSOR:
            self._record_sensor_notification(options, received_at_ns)
            return
        if source is Service.SAFETY and command == _SafetyCommand.REPORT_FAULT:
            with self._state_lock:
                self._faults = PowerStageFault(options)
            return
        if source is Service.LAUNCH and command == _LaunchCommand.RUN_STATUS:
            try:
                status = LaunchStatus(options)
            except ValueError as error:
                raise BoardProtocolError(
                    f"invalid launch status {options}"
                ) from error
            with self._sensor_condition:
                self._launch_status = status
                self._launch_active = False
                if (
                    self._timing.active
                    and self._timing.end_reference_ns == 0
                ):
                    self._timing.end_reference_ns = received_at_ns
                self._launch_status_event.set()
                self._sensor_condition.notify_all()
            return

        with self._state_lock:
            pending = self._pending
            if (
                pending is None
                or pending.service is not source
                or pending.command != command
            ):
                return
            pending.response = options
            if self._timing.active and pending.sent_at_ns:
                self._timing.requests.append(
                    _HostRequestTiming(
                        pending.service,
                        pending.command,
                        pending.options,
                        pending.sent_at_ns,
                        received_at_ns,
                    )
                )
            self._last_status[(source, command)] = options
            self._pending = None
            pending.event.set()

    def _record_sensor_notification(self, options: int, received_at_ns: int) -> None:
        if options & ~0x07:
            raise BoardProtocolError(f"invalid sensor options 0x{options:02X}")
        notification = SensorNotification(
            SensorChannel(options & 0x03),
            SensorEdge.FALLING if options & 0x04 else SensorEdge.RISING,
            received_at_ns / 1_000_000_000.0,
        )
        with self._sensor_condition:
            self._sensor_notifications.append(notification)
            if self._timing.active:
                self._timing.sensors.append(notification)
            self._sensor_condition.notify_all()

    def _handle_launch_frame(self, header: bytes, payload: bytes) -> None:
        if (
            header[1] != _LAUNCH_VERSION
            or header[6] != len(payload)
            or header[7] != 0
        ):
            self._set_launch_data_error(BoardProtocolError("invalid LaunchData header"))
            return
        frame_type = header[2]
        run_id = header[3]
        chunk = int.from_bytes(header[4:6], "little")

        if frame_type == _LAUNCH_DATA_FRAME:
            if chunk == 0:
                self._launch_rx_run_id = run_id
                self._launch_rx_chunk = 0
                self._launch_rx_payload.clear()
            if (
                self._launch_rx_run_id != run_id
                or chunk != self._launch_rx_chunk
            ):
                self._set_launch_data_error(
                    BoardProtocolError("LaunchData chunk sequence mismatch")
                )
                return
            self._launch_rx_payload.extend(payload)
            self._launch_rx_chunk += 1
            return

        if frame_type != _LAUNCH_FINAL_FRAME or len(payload) != 8:
            self._set_launch_data_error(BoardProtocolError("invalid LaunchData frame"))
            return
        if self._launch_rx_run_id != run_id or chunk != self._launch_rx_chunk:
            self._set_launch_data_error(
                BoardProtocolError("LaunchData final sequence mismatch")
            )
            return

        expected_size, expected_crc = struct.unpack("<II", payload)
        actual_crc = zlib.crc32(self._launch_rx_payload) & 0xFFFFFFFF
        if expected_size != len(self._launch_rx_payload):
            self._set_launch_data_error(BoardProtocolError("LaunchData length mismatch"))
            return
        if expected_crc != actual_crc:
            self._set_launch_data_error(BoardProtocolError("LaunchData CRC failed"))
            return
        try:
            decoded = _decode_launch_data(
                run_id,
                self._sampling_frequency_hz,
                bytes(self._launch_rx_payload),
            )
        except (ValueError, struct.error) as error:
            self._set_launch_data_error(BoardProtocolError(str(error)))
            return
        with self._state_lock:
            self._launch_data = decoded
            self._launch_data_error = None
            self._launch_data_event.set()

    def _wait_for_launch_status(self, timeout_s: float) -> LaunchStatus:
        if not self._launch_status_event.wait(timeout_s):
            raise BoardTimeoutError("timed out waiting for final launch status")
        with self._state_lock:
            if self._launch_status is None:
                self._raise_background_error_locked()
                raise BoardProtocolError("launch status event contained no status")
            return self._launch_status

    def _wait_for_launch_data(self, timeout_s: float) -> LaunchData:
        if not self._launch_data_event.wait(timeout_s):
            raise BoardTimeoutError("timed out waiting for LaunchData")
        with self._state_lock:
            if self._launch_data_error is not None:
                raise self._launch_data_error
            if self._launch_data is None:
                self._raise_background_error_locked()
                raise BoardProtocolError("LaunchData event contained no data")
            if self._launch_data.run_id != self._active_run_id:
                raise BoardProtocolError("LaunchData run ID does not match")
            return self._launch_data

    def _set_launch_data_error(self, error: Exception) -> None:
        with self._state_lock:
            self._launch_data_error = error
            self._launch_data_event.set()

    def _reset_launch_receiver(self) -> None:
        self._launch_rx_run_id = None
        self._launch_rx_chunk = 0
        self._launch_rx_payload.clear()

    def _fail_background(self, error: Exception) -> None:
        wrapped = (
            error
            if isinstance(error, BoardError)
            else BoardError(f"USB receiver stopped: {error}")
        )
        with self._sensor_condition:
            self._background_error = wrapped
            if self._pending is not None:
                self._pending.event.set()
            self._launch_status_event.set()
            self._launch_data_event.set()
            self._sensor_condition.notify_all()

    def _raise_background_error_locked(self) -> None:
        if self._background_error is not None:
            raise self._background_error

    def _ensure_open(self) -> None:
        if self._closed:
            raise BoardError("board connection is closed")

    def close(self) -> None:
        if self._closed:
            return
        try:
            with self._state_lock:
                launch_active = self._launch_active
            if launch_active:
                self.abortLaunch()
            self.sleep(Bridge.ALL)
        except BoardError:
            pass
        finally:
            # Closing the port is the last-resort signal for firmware to enter
            # its safe reset path, even if Ctrl+C interrupts the abort itself.
            self._closed = True
            self._stop_event.set()
            self._receiver_generation += 1
            self._serial.close()
            self._receiver.join(timeout=1.0)

    def __enter__(self) -> "CompetitionBoard":
        return self

    def __exit__(self, _exc_type, _exc_value, _traceback) -> None:
        self.close()


# Nom francais de la classe principale. CompetitionBoard demeure disponible.
CarteCompetition = CompetitionBoard


def _decode_launch_data(
    run_id: int, sampling_frequency_hz: int, payload: bytes
) -> LaunchData:
    offset = 0

    def take(size: int) -> bytes:
        nonlocal offset
        end = offset + size
        if end > len(payload):
            raise ValueError("truncated LaunchData payload")
        value = payload[offset:end]
        offset = end
        return value

    sensor_events: list[SensorEvents] = []
    for _ in range(4):
        rising_count, falling_count = struct.unpack("<BB", take(2))
        if rising_count > _SENSOR_CAPACITY or falling_count > _SENSOR_CAPACITY:
            raise ValueError("invalid sensor event count")
        rising = (
            struct.unpack(f"<{rising_count}I", take(4 * rising_count))
            if rising_count
            else ()
        )
        falling = (
            struct.unpack(f"<{falling_count}I", take(4 * falling_count))
            if falling_count
            else ()
        )
        sensor_events.append(SensorEvents(tuple(rising), tuple(falling)))

    (current_count,) = struct.unpack("<I", take(4))
    if current_count > 25_000:
        raise ValueError("invalid current sample count")
    current_bytes = take(current_count * 4)
    current_samples = tuple(struct.iter_unpack("<BBBB", current_bytes))

    power_count, missed_power = struct.unpack("<II", take(8))
    if power_count > 10_000:
        raise ValueError("invalid power sample count")
    power_samples = (
        struct.unpack(f"<{power_count}H", take(power_count * 2))
        if power_count
        else ()
    )
    launch_start, launch_end, velocity_tick_delta = struct.unpack(
        "<III", take(12)
    )
    (snapshot_count,) = struct.unpack("<H", take(2))
    if snapshot_count > _SNAPSHOT_CAPACITY:
        raise ValueError("invalid request snapshot count")
    request_snapshots: list[RequestSnapshot] = []
    for _ in range(snapshot_count):
        service, command, options, flags, created, outgoing, done = (
            struct.unpack("<BBBBIII", take(16))
        )
        if flags & ~RequestSnapshot.HAS_OUTGOING_TIMESTAMP:
            raise ValueError(f"invalid request snapshot flags 0x{flags:02X}")
        try:
            event_service = Service(service)
        except ValueError as error:
            raise ValueError(
                f"invalid request snapshot service {service}"
            ) from error
        request_snapshots.append(
            RequestSnapshot(
                event_service,
                command,
                options,
                created,
                outgoing if flags & RequestSnapshot.HAS_OUTGOING_TIMESTAMP else None,
                done,
            )
        )
    if offset != len(payload):
        raise ValueError("LaunchData contains trailing bytes")
    return LaunchData(
        run_id,
        sampling_frequency_hz,
        tuple(sensor_events),
        current_samples,
        tuple(power_samples),
        missed_power,
        launch_start,
        launch_end,
        velocity_tick_delta,
        tuple(request_snapshots),
    )
