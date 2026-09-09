# API du technicien

L'API complète utilise principalement `CompetitionBoard` et `DeviceMode.DEBUG` :

```python
from dda_host import CompetitionBoard, DeviceMode

with CompetitionBoard.connect(mode=DeviceMode.DEBUG) as carte:
    print(carte.getSystemState())
    print(carte.getFaults())
```

La connexion sans argument détecte automatiquement l'unique carte DDA.

## Fonctions de diagnostic principales

| Fonction | Utilisation |
| --- | --- |
| `getFaults()` | Lire le masque des défauts mémorisés. |
| `getSystemState()` | Lire l'état global du système. |
| `getCurrent(H1..H4)` | Lire la consigne de courant d'un pont. |
| `setCurrent(mA, pont)` | Modifier une ou plusieurs consignes. |
| `inverserDirection(H1..H4)` | Inverser localement les futures commandes de direction; rappeler la méthode annule l'inversion. |
| `getPmode()` / `setPmode()` | Lire ou régler le mode commun des DRV8874. |
| `setSamplingFrequency(Hz)` | Régler l'acquisition entre 100 et 5000 Hz. |
| `formatTimingReport()` | Afficher les statistiques temporelles de débogage. |
| `plotLaunchResult()` | Tracer les courants, la puissance et les événements. |

Les commandes de pont, capteur et lancement de l'API participant restent aussi
disponibles. Le mode débogage ne désactive aucune protection matérielle ou
logicielle. Contrairement au mode compétition, il autorise les commandes de
ponts et de capteurs même lorsqu'aucun lancement n'est actif.
