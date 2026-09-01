# API Python des participants

## Présentation

L'API permet au programme d'un participant de commander les quatre bobines,
d'attendre les signaux des capteurs et de gérer une tentative de lancement.
La communication USB, l'initialisation de la carte, la collecte des mesures et
la remise en état sécuritaire sont prises en charge par le programme
organisateur.

Le participant modifie uniquement la fonction `participant_code` du fichier
`examples/competition.py`. L'objet reçu par cette fonction est une
`CarteCompetition` déjà connectée, configurée et prête à être utilisée.

```python
from dda_host import Bobine, Capteur, CarteCompetition, Direction


def participant_code(carte: CarteCompetition):
    carte.demarrerLancement()
    carte.reglerCourant(2000)

    carte.activer(Bobine.H1, Direction.AVANT)
    carte.attendreCapteur(Capteur.CAPTEUR_1)
    carte.desactiver(Bobine.H1)

    return carte.arreterLancement()
```

## Énumérations publiques

### `Bobine`

| Valeur | Signification |
| --- | --- |
| `Bobine.H1` | Bobine reliée au canal H1 |
| `Bobine.H2` | Bobine reliée au canal H2 |
| `Bobine.H3` | Bobine reliée au canal H3 |
| `Bobine.H4` | Bobine reliée au canal H4 |
| `Bobine.TOUTES` | Toutes les bobines |

### `Direction`

| Valeur | Signification |
| --- | --- |
| `Direction.AVANT` | Courant dans le sens avant |
| `Direction.ARRIERE` | Courant dans le sens arrière |

### `Capteur`

| Valeur | Signification |
| --- | --- |
| `Capteur.CAPTEUR_1` | Capteur 1 |
| `Capteur.CAPTEUR_2` | Capteur 2 |
| `Capteur.CAPTEUR_3` | Capteur 3 |
| `Capteur.CAPTEUR_4` | Capteur 4 |
| `Capteur.TOUS` | N'importe quel capteur ou tous les capteurs |

### `FrontCapteur`

| Valeur | Signification |
| --- | --- |
| `FrontCapteur.MONTANT` | Transition du niveau bas vers le niveau haut |
| `FrontCapteur.DESCENDANT` | Transition du niveau haut vers le niveau bas |

Les autres énumérations publiques sont `ModeCarte`, `EtatSysteme`,
`EtatLancement` et `DefautEtagePuissance`. Elles servent surtout au programme
organisateur et aux diagnostics.

## Méthodes publiques de `CarteCompetition`

### Connexion et configuration

| Méthode | Description |
| --- | --- |
| `connecter(port=None, ...)` | Ouvre une carte. Sans port, la carte USB est détectée automatiquement. |
| `reglerMode(mode)` | Sélectionne le mode débogage ou compétition. |
| `reglerFrequenceEchantillonnage(frequence_hz)` | Règle l'acquisition de 100 à 5000 Hz, par pas de 100 Hz. |
| `fermer()` | Ferme la connexion et demande une remise en état sécuritaire. |

Le programme organisateur réalise normalement lui-même la connexion et la
configuration. Le participant reçoit donc directement une carte prête à
l'emploi.

### Gestion d'un lancement

| Méthode | Description |
| --- | --- |
| `demarrerLancement(identifiant=None)` | Démarre une tentative et retourne son identifiant. L'identifiant facultatif doit être compris entre 0 et 127. |
| `arreterLancement(delai_s=None)` | Termine normalement la tentative, attend les mesures et retourne un `ResultatLancement`. |
| `annulerLancement(delai_s=None)` | Interrompt immédiatement la tentative active et retourne son état final. |

### Commande des bobines

| Méthode | Description |
| --- | --- |
| `reglerCourant(courant_ma, bobine=Bobine.TOUTES)` | Règle le courant de la bobine choisie, ou de toutes les bobines, entre 0 et 3000 mA par pas de 25 mA. |
| `lireCourant(bobine)` | Lit le courant configuré d'une bobine en mA. `Bobine.TOUTES` n'est pas accepté. |
| `reglerPmode(mode_pwm)` | Sélectionne le mode PWM (`True`) ou PH/EN (`False`); tous les pilotes doivent d'abord être en veille. |
| `lirePmode()` | Retourne `True` lorsque le mode PWM partagé est configuré. |
| `activer(bobine, direction)` | Alimente la bobine sélectionnée dans la direction demandée. |
| `desactiver(bobine=Bobine.TOUTES)` | Coupe le courant sans mettre le pilote électronique en veille. |
| `reveiller(bobine=Bobine.TOUTES)` | Réveille le pilote électronique de la bobine. |
| `mettreEnVeille(bobine=Bobine.TOUTES)` | Met le pilote électronique de la bobine en veille. |

### Lecture des capteurs

| Méthode | Description |
| --- | --- |
| `attendreCapteur(capteur=Capteur.TOUS, front=None, delai_s=None)` | Attend une notification correspondant au capteur et au front demandés. |
| `prendreNotificationsCapteurs()` | Retourne toutes les notifications en attente, puis vide la file. |
| `deverrouillerCapteurs()` | Réarme les notifications provenant des capteurs. |

`attendreCapteur` retourne une `NotificationCapteur` avec les propriétés
`capteur`, `front` et `recu_a_s`. Si `delai_s` est fourni et qu'aucun signal ne
survient à temps, la méthode lève `ErreurDelaiCarte`.

Exemple avec filtrage du front et délai maximal :

```python
from dda_host import Capteur, ErreurDelaiCarte, FrontCapteur

try:
    notification = carte.attendreCapteur(
        Capteur.CAPTEUR_2,
        front=FrontCapteur.MONTANT,
        delai_s=2.0,
    )
except ErreurDelaiCarte:
    carte.annulerLancement()
```

### Étalonnage des capteurs

| Méthode | Description |
| --- | --- |
| `demarrerEtalonnageCapteurs(delai_s=10.0)` | Lance l'étalonnage des quatre capteurs et attend sa sauvegarde. |
| `lireEtalonnageCapteur(capteur)` | Lit l'étalonnage sauvegardé d'un capteur. |
| `etalonnerCapteurs(delai_s=10.0)` | Étalonne les quatre capteurs et retourne toutes les valeurs sauvegardées. |
| `utiliserNiveauxCapteursParDefaut(capteur=Capteur.TOUS)` | Applique les niveaux de détection par défaut. |
| `utiliserNiveauxCapteursEtalonnes(capteur=Capteur.TOUS)` | Applique les niveaux issus de l'étalonnage. |

Un `EtalonnageCapteur` expose les propriétés `capteur`, `code_courant_del` et
`code_seuil_declenchement`.

### État et diagnostic

| Méthode ou propriété | Description |
| --- | --- |
| `lireDefauts()` | Interroge la carte et retourne les défauts de l'étage de puissance. |
| `lireEtatSysteme()` | Interroge la carte et retourne son état de fonctionnement. |
| `defauts` | Derniers défauts reçus ou lus. |
| `etatLancement` | État final reçu pour la tentative courante. |
| `lancementActif` | Indique si une tentative est active. |
| `erreurArrierePlan` | Dernière erreur provenant de la réception asynchrone. |

## Résultat d'un lancement

`arreterLancement` retourne un `ResultatLancement` possédant les propriétés
suivantes :

| Propriété | Description |
| --- | --- |
| `etat` | État final de type `EtatLancement`. |
| `donnees` | Mesures de type `DonneesLancement`, ou `None` si elles ne sont pas disponibles. |
| `rapport_temporisation` | Rapport de temporisation produit en mode débogage. |

Les propriétés principales de `DonneesLancement` sont `identifiant`,
`frequence_echantillonnage_hz`, `evenements_capteurs`, `duree_us` et
`vitesse_m_s`.

## Fonctions utilitaires publiques

| Fonction | Description |
| --- | --- |
| `formaterRapportTemporisation(rapport)` | Produit une chaîne lisible contenant les statistiques temporelles du mode débogage. |
| `tracerResultatLancement(resultat)` | Affiche les graphiques des mesures et, en mode débogage, des latences. |

## Erreurs publiques

| Erreur | Description |
| --- | --- |
| `ErreurCarte` | Erreur générale de la carte ou de la communication. |
| `ErreurProtocoleCarte` | Trame reçue invalide ou incohérente. |
| `ErreurCommandeRejetee` | Commande refusée par le micrologiciel. |
| `ErreurDelaiCarte` | Réponse, mesure ou capteur non reçu dans le délai prévu. |

## Exécution du programme

Depuis le dossier `Host` :

```powershell
python run_competition.py
```

Si la détection automatique ne trouve pas la carte, le port série peut être
précisé :

```powershell
python run_competition.py --port COM7
```

Le programme organisateur intercepte les interruptions et les erreurs de
communication. Lors de la fermeture, il tente d'annuler tout lancement actif
et de mettre toutes les bobines en veille.
