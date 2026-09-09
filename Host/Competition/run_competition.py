"""Détecte la carte DDA et lance le programme du participant."""

from __future__ import annotations

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from competition import participant_code
from dda_host import (
    Bobine,
    CarteCompetition,
    ErreurCarte,
    ErreurDelaiCarte,
    EtatLancement,
    ResultatLancement,
    tracerResultatLancement,
)


def executer_programme(carte: CarteCompetition) -> ResultatLancement | None:
    try:
        return participant_code(carte)
    except KeyboardInterrupt:
        print("Interruption du participant; arrêt sécuritaire en cours...")
        if carte.lancementActif:
            try:
                return carte.annulerLancement()
            except ErreurCarte as erreur:
                print(f"Impossible de confirmer l'arrêt: {erreur}", file=sys.stderr)
        return None
    except ErreurDelaiCarte:
        if carte.etatLancement is not EtatLancement.DELAI_DEPASSE:
            raise
        print("Délai de lancement dépassé; récupération des mesures...")
        return carte.arreterLancement()


def main() -> None:
    try:
        with CarteCompetition.connecter() as carte:
            carte.reglerFrequenceEchantillonnage(5000)
            resultat = executer_programme(carte)
            if resultat is None:
                return
            if resultat.donnees is not None:
                print(f"Vitesse : {resultat.donnees.vitesse_m_s:.2f} m/s")
            tracerResultatLancement(resultat)
    except ErreurCarte as erreur:
        print(f"Erreur de communication avec la carte : {erreur}", file=sys.stderr)


if __name__ == "__main__":
    main()
