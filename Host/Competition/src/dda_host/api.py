"""API publique minimale remise aux participants."""

from __future__ import annotations

from typing import Optional

from ._client import (
    Bobine,
    Capteur,
    CompetitionBoard as _Client,
    DeviceMode,
    Direction,
    DonneesLancement,
    EtatLancement,
    FrontCapteur,
    NotificationCapteur,
    ResultatLancement,
    plotLaunchResult,
)


class CarteCompetition:
    """Façade limitée aux commandes utiles pendant une compétition."""

    __slots__ = ("__client",)

    def __init__(self, client: _Client) -> None:
        self.__client = client

    @classmethod
    def connecter(cls) -> "CarteCompetition":
        """Détecte l'unique carte USB et force le mode compétition."""

        return cls(_Client.connect(mode=DeviceMode.COMPETITION))

    @property
    def etatLancement(self) -> Optional[EtatLancement]:
        return self.__client.etatLancement

    @property
    def lancementActif(self) -> bool:
        return self.__client.lancementActif

    def reglerFrequenceEchantillonnage(self, frequence_hz: int) -> None:
        self.__client.reglerFrequenceEchantillonnage(frequence_hz)

    def demarrerLancement(self, identifiant: Optional[int] = None) -> int:
        identifiant_lancement = self.__client.demarrerLancement(identifiant)
        try:
            self.__client.reglerCourant(1000)
            self.__client.utiliserNiveauxCapteursParDefaut()
            self.__client.reveiller(Bobine.TOUTES)
        except BaseException:
            try:
                self.__client.annulerLancement()
            finally:
                raise
        return identifiant_lancement

    def arreterLancement(
        self, delai_s: Optional[float] = None
    ) -> ResultatLancement:
        return self.__client.arreterLancement(delai_s)

    def annulerLancement(
        self, delai_s: Optional[float] = None
    ) -> ResultatLancement:
        return self.__client.annulerLancement(delai_s)

    def reglerCourant(
        self, courant_ma: int, bobine: Bobine | int = Bobine.TOUTES
    ) -> None:
        self.__client.reglerCourant(courant_ma, bobine)

    def inverserDirection(self, bobine: Bobine | int) -> None:
        self.__client.inverserDirection(bobine)

    def activer(self, bobine: Bobine | int, direction: Direction | str) -> None:
        self.__client.activer(bobine, direction)

    def desactiver(self, bobine: Bobine | int = Bobine.TOUTES) -> None:
        self.__client.desactiver(bobine)

    def reveiller(self, bobine: Bobine | int = Bobine.TOUTES) -> None:
        self.__client.reveiller(bobine)

    def mettreEnVeille(self, bobine: Bobine | int = Bobine.TOUTES) -> None:
        self.__client.mettreEnVeille(bobine)

    def attendreCapteur(
        self,
        capteur: Capteur | int = Capteur.TOUS,
        front: Optional[FrontCapteur | str] = None,
        delai_s: Optional[float] = None,
    ) -> NotificationCapteur:
        return self.__client.attendreCapteur(capteur, front, delai_s)

    def prendreNotificationsCapteurs(
        self,
    ) -> tuple[NotificationCapteur, ...]:
        return self.__client.prendreNotificationsCapteurs()

    def deverrouillerCapteurs(self) -> None:
        self.__client.deverrouillerCapteurs()

    def utiliserNiveauxCapteursParDefaut(
        self, capteur: Capteur | int = Capteur.TOUS
    ) -> None:
        self.__client.utiliserNiveauxCapteursParDefaut(capteur)

    # Conservé pour ne pas casser le programme participant existant.
    useDefaultSensorLevels = utiliserNiveauxCapteursParDefaut

    def fermer(self) -> None:
        self.__client.fermer()

    def __enter__(self) -> "CarteCompetition":
        return self

    def __exit__(self, _type, _value, _traceback) -> None:
        self.fermer()


def tracerResultatLancement(resultat: ResultatLancement) -> None:
    """Affiche les mesures principales du lancement."""

    plotLaunchResult(resultat)
