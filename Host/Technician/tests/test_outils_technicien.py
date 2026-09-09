"""Tests sans matériel des procédures du technicien."""

from __future__ import annotations

import unittest
from unittest.mock import patch

from dda_host import (
    BoardRejectedError,
    Bridge,
    LaunchResult,
    LaunchStatus,
    PowerStageFault,
)
from outils_technicien import tester_ponts_h


class FausseCarte:
    def __init__(self) -> None:
        self.appels: list[tuple[object, ...]] = []
        self.launch_active = False
        self.courant_ma = 0
        self.refuser_dernier_arret = False
        self._lectures_defauts = iter(
            (PowerStageFault.NONE, PowerStageFault.DRIVER_H2)
        )
        self.resultat = LaunchResult(LaunchStatus.SAFETY_FAULT, object())

    def startLaunch(self) -> None:
        self.appels.append(("start",))
        self.launch_active = True

    def setCurrent(self, courant_ma: int) -> None:
        self.appels.append(("current", courant_ma))
        self.courant_ma = courant_ma

    def wake(self, pont: Bridge) -> None:
        self.appels.append(("wake", pont))

    def forward(self, pont: Bridge) -> None:
        self.appels.append(("forward", pont))

    def off(self, pont: Bridge) -> None:
        self.appels.append(("off", pont))
        if (
            self.refuser_dernier_arret
            and self.courant_ma == 2000
            and pont is Bridge.ALL
        ):
            raise BoardRejectedError("déjà en défaut")

    def getFaults(self) -> PowerStageFault:
        self.appels.append(("faults",))
        return next(self._lectures_defauts)

    def stopLaunch(self) -> LaunchResult:
        self.appels.append(("stop",))
        self.launch_active = False
        return self.resultat

    def abortLaunch(self) -> LaunchResult:
        self.launch_active = False
        return LaunchResult(LaunchStatus.HOST_ABORTED, None)


class TestsOutilsTechnicien(unittest.TestCase):
    @patch("outils_technicien.time.sleep")
    def test_sequence_1a_puis_2a(self, _attente) -> None:
        carte = FausseCarte()
        resultat = tester_ponts_h(  # type: ignore[arg-type]
            carte, afficher=lambda _message: None
        )

        self.assertTrue(resultat.reussi)
        self.assertEqual(
            [appel for appel in carte.appels if appel[0] == "current"],
            [("current", 1000), ("current", 2000)],
        )
        self.assertEqual(
            [appel[1] for appel in carte.appels if appel[0] == "forward"],
            [Bridge.H1, Bridge.H2, Bridge.H3, Bridge.H4, Bridge.ALL] * 2,
        )
        self.assertEqual(carte.appels[-1], ("stop",))

    @patch("outils_technicien.time.sleep")
    def test_defaut_attendu_ne_bloque_pas_le_rapport(self, _attente) -> None:
        carte = FausseCarte()
        carte.refuser_dernier_arret = True

        resultat = tester_ponts_h(  # type: ignore[arg-type]
            carte, afficher=lambda _message: None
        )

        self.assertTrue(resultat.reussi)
        self.assertEqual(carte.appels[-1], ("stop",))


if __name__ == "__main__":
    unittest.main()
