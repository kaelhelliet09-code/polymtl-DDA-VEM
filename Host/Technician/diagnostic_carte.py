"""Affiche l'état non destructif de la carte DDA."""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from dda_host import BoardError, Bridge, CompetitionBoard, DeviceMode
from outils_technicien import noms_defauts


def main() -> int:
    try:
        with CompetitionBoard.connect(mode=DeviceMode.DEBUG) as carte:
            print(f"État système : {carte.getSystemState().name}")
            print(f"Défauts : {noms_defauts(carte.getFaults())}")
            print(f"PMODE PWM : {carte.getPmode()}")
            for pont in (Bridge.H1, Bridge.H2, Bridge.H3, Bridge.H4):
                print(f"Courant {pont.name} : {carte.getCurrent(pont)} mA")
        return 0
    except BoardError as erreur:
        print(f"Diagnostic interrompu : {erreur}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
