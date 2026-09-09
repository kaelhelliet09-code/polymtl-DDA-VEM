# API du participant

Le participant reçoit un objet `CarteCompetition` déjà connecté et configuré.
Il ne doit pas ouvrir directement le port série.

```python
from dda_host import Bobine, Capteur, CarteCompetition, Direction


def participant_code(carte: CarteCompetition):
    carte.inverserDirection(Bobine.H2)  # Si H2 est branchée à l'envers.
    carte.demarrerLancement()
    carte.reglerCourant(2000)
    carte.activer(Bobine.H1, Direction.AVANT)
    carte.attendreCapteur(Capteur.CAPTEUR_1)
    carte.desactiver(Bobine.H1)
    return carte.arreterLancement()
```

En mode compétition, toutes les commandes de ponts et de configuration des
capteurs doivent être envoyées pendant un lancement actif. Elles sont refusées
avant `demarrerLancement()` et après la fin du lancement. Les commandes qui
démarrent, arrêtent ou annulent le lancement restent toujours disponibles.

## Commandes disponibles

| Méthode | Description |
| --- | --- |
| `demarrerLancement()` | Commence l'enregistrement, applique 1 A et les niveaux de capteurs par défaut, puis réveille les ponts. |
| `arreterLancement()` | Termine le lancement et retourne ses mesures. |
| `annulerLancement()` | Interrompt immédiatement le lancement. |
| `reglerCourant(mA, bobine)` | Règle un seuil de 0 à 3000 mA, par pas de 25 mA. |
| `inverserDirection(bobine)` | Inverse localement AVANT et ARRIERE pour cette bobine. Un second appel annule l'inversion. |
| `activer(bobine, direction)` | Commande une bobine vers l'avant ou l'arrière. |
| `desactiver(bobine)` | Coupe le courant sans mettre le pilote en veille. |
| `reveiller(bobine)` | Réveille un ou tous les pilotes. |
| `mettreEnVeille(bobine)` | Met un ou tous les pilotes en veille. |
| `attendreCapteur(capteur, front, delai_s)` | Attend le signal demandé. |
| `prendreNotificationsCapteurs()` | Lit et vide les notifications en attente. |
| `deverrouillerCapteurs()` | Réarme les notifications des capteurs. |
| `utiliserNiveauxCapteursParDefaut()` | Applique les seuils de capteur du firmware. |

Les sélections possibles sont `Bobine.H1` à `Bobine.H4`, `Bobine.TOUTES`,
`Capteur.CAPTEUR_1` à `Capteur.CAPTEUR_4`, `Capteur.TOUS`,
`Direction.AVANT`, `Direction.ARRIERE`, `FrontCapteur.MONTANT` et
`FrontCapteur.DESCENDANT`.

`inverserDirection()` Permet d'inverser la définition de la direction pour qu'il concorde avec le branchment des bobines fais par le participants. Il n'envoie rien à la carte : elle peut donc être appelée
avant `demarrerLancement()`. Le réglage reste actif jusqu'à la fermeture du
programme Python. `Bobine.TOUTES` inverse les quatre bobines.  
Exemple si le participants voit que la direction `AVANT` est entrain de pousser son véhicule vers le début il peut utiliser `inverserDirection()`afin que `AVANT` corresponde avec un mouvment vers l'avant 

Les erreurs publiques sont `ErreurCarte`, `ErreurProtocoleCarte`,
`ErreurCommandeRejetee` et `ErreurDelaiCarte`.
