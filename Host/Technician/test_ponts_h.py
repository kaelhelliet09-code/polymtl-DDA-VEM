"""Qualifie les quatre ponts en H avec la barre de résistances de 6,9 ohms."""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from dda_host import (
    BoardError,
    CompetitionBoard,
    DeviceMode,
    formatTimingReport,
    plotLaunchResult,
)
from outils_technicien import tester_ponts_h


def main() -> int:
    input(
        "Branchez une résistance de 6,9 Ω sur chaque pont, utilisez "
        "l'alimentation protégée, puis appuyez sur Entrée."
    )
    try:
        with CompetitionBoard.connect(mode=DeviceMode.DEBUG) as carte:
            carte.setSamplingFrequency(5000)
            resultat = tester_ponts_h(carte)
            if resultat.lancement.timing_report is not None:
                print(formatTimingReport(resultat.lancement.timing_report))
            plotLaunchResult(resultat.lancement)
            return 0 if resultat.reussi else 1
    except BoardError as erreur:
        print(f"Test interrompu : {erreur}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
