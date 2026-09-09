"""Vérifie manuellement la réception d'un événement de chaque capteur."""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from dda_host import BoardError, CompetitionBoard, DeviceMode, SensorChannel


def main() -> int:
    try:
        with CompetitionBoard.connect(mode=DeviceMode.DEBUG) as carte:
            carte.useDefaultSensorLevels()
            carte.takeSensorNotifications()
            carte.unlockSensor()
            for capteur in (
                SensorChannel.SENSOR_1,
                SensorChannel.SENSOR_2,
                SensorChannel.SENSOR_3,
                SensorChannel.SENSOR_4,
            ):
                input(f"Déclenchez le capteur {int(capteur) + 1}, puis Entrée.")
                evenement = carte.waitForSensor(capteur, timeout_s=10.0)
                print(f"  Reçu : {evenement.sensor.name}, {evenement.edge.value}")
        print("Test des capteurs RÉUSSI.")
        return 0
    except BoardError as erreur:
        print(f"Test interrompu : {erreur}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
