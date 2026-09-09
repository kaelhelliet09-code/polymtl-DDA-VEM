"""Procédures matérielles réutilisables par les scripts du technicien."""

from __future__ import annotations

from dataclasses import dataclass
import time
from typing import Callable

from dda_host import (
    BoardError,
    BoardRejectedError,
    Bridge,
    CompetitionBoard,
    LaunchResult,
    PowerStageFault,
)


@dataclass(frozen=True, slots=True)
class ResultatTestPonts:
    lancement: LaunchResult
    defauts_1a: PowerStageFault
    defauts_2a: PowerStageFault

    @property
    def reussi(self) -> bool:
        return (
            self.defauts_1a == PowerStageFault.NONE
            and self.defauts_2a != PowerStageFault.NONE
        )


def noms_defauts(defauts: PowerStageFault) -> str:
    if defauts == PowerStageFault.NONE:
        return "aucun"
    noms = {
        PowerStageFault.DRIVER_H1: "pilote H1",
        PowerStageFault.DRIVER_H2: "pilote H2",
        PowerStageFault.DRIVER_H3: "pilote H3",
        PowerStageFault.DRIVER_H4: "pilote H4",
        PowerStageFault.POWER_ALERT: "alerte de puissance",
        PowerStageFault.SAFE_STATE_FAILURE: "échec de la mise en sécurité",
    }
    return ", ".join(
        noms.get(defaut, defaut.name)
        for defaut in PowerStageFault
        if defaut is not PowerStageFault.NONE and defauts & defaut
    )


def _exercer_ponts(
    carte: CompetitionBoard,
    duree_s: float,
    afficher: Callable[[str], None],
) -> None:
    for pont in (Bridge.H1, Bridge.H2, Bridge.H3, Bridge.H4, Bridge.ALL):
        nom = "tous les ponts" if pont is Bridge.ALL else pont.name
        afficher(f"  Activation de {nom}...")
        try:
            carte.forward(pont)
        except BoardRejectedError as erreur:
            afficher(f"  Activation refusée après un défaut : {erreur}")
            return
        time.sleep(duree_s)
        try:
            carte.off(pont)
        except BoardRejectedError as erreur:
            afficher(f"  Pont déjà coupé par la sécurité : {erreur}")
            return


def tester_ponts_h(
    carte: CompetitionBoard,
    *,
    duree_s: float = 0.5,
    afficher: Callable[[str], None] = print,
) -> ResultatTestPonts:
    """Exécute la procédure existante à 1 A puis à 2 A."""

    if duree_s < 0:
        raise ValueError("duree_s ne doit pas être négative")

    carte.startLaunch()
    try:
        afficher("Test de chaque pont à 1 A...")
        carte.setCurrent(1000)
        carte.wake(Bridge.ALL)
        _exercer_ponts(carte, duree_s, afficher)
        defauts_1a = carte.getFaults()
        afficher(f"Défauts à 1 A : {noms_defauts(defauts_1a)}")

        defauts_2a = PowerStageFault.NONE
        if defauts_1a == PowerStageFault.NONE:
            afficher("Test de chaque pont à 2 A...")
            carte.setCurrent(2000)
            _exercer_ponts(carte, duree_s, afficher)
            defauts_2a = carte.getFaults()
            afficher(f"Défauts à 2 A : {noms_defauts(defauts_2a)}")
        else:
            afficher("Test à 2 A ignoré à cause du défaut présent à 1 A.")

        lancement = carte.stopLaunch()
    except BaseException:
        if carte.launch_active:
            try:
                carte.abortLaunch()
            except BoardError:
                pass
        raise

    resultat = ResultatTestPonts(lancement, defauts_1a, defauts_2a)
    afficher("Test des ponts RÉUSSI." if resultat.reussi else "Test des ponts ÉCHOUÉ.")
    return resultat
