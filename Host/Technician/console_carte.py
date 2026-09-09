"""Console Python interactive pour envoyer directement des commandes à la carte."""

from code import interact
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from dda_host import (
    BoardError,
    Bridge,
    CompetitionBoard,
    DeviceMode,
    Direction,
    SensorChannel,
    SensorEdge,
)


def main() -> int:
    try:
        with CompetitionBoard.connect(mode=DeviceMode.DEBUG) as carte:
            interact(
                banner=(
                    "Carte connectée. Utilisez par exemple :\n"
                    "  carte.reveiller(H1)\n"
                    "  carte.reglerCourant(1000, H1)\n"
                    "  carte.activer(H1, AVANT)\n"
                    "  carte.desactiver(H1)\n"
                    "  carte.lireDefauts()\n"
                    "  carte.utiliserNiveauxCapteursParDefaut()\n"
                    "  carte.deverrouillerCapteurs()\n"
                    "  carte.attendreCapteur(C1, MONTANT, 10.0)\n"
                    "Tapez exit() pour fermer la connexion."
                ),
                local={
                    "carte": carte,
                    "H1": Bridge.H1,
                    "H2": Bridge.H2,
                    "H3": Bridge.H3,
                    "H4": Bridge.H4,
                    "TOUTES": Bridge.ALL,
                    "AVANT": Direction.FORWARD,
                    "ARRIERE": Direction.REVERSE,
                    "C1": SensorChannel.SENSOR_1,
                    "C2": SensorChannel.SENSOR_2,
                    "C3": SensorChannel.SENSOR_3,
                    "C4": SensorChannel.SENSOR_4,
                    "TOUS_CAPTEURS": SensorChannel.ALL,
                    "MONTANT": SensorEdge.RISING,
                    "DESCENDANT": SensorEdge.FALLING,
                },
                exitmsg="Fermeture sécuritaire de la carte...",
            )
        return 0
    except BoardError as erreur:
        print(f"Connexion impossible : {erreur}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
