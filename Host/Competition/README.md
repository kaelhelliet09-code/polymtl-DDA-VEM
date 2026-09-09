# Programme de compétition DDA

Ce dossier contient uniquement le programme du participant, le lanceur et
l'API nécessaire à une compétition. Les fonctions de diagnostic, d'étalonnage
et de test matériel se trouvent dans le dossier `Technician`.

## Installation

Python 3.10 ou une version plus récente est requis. Depuis ce dossier :

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -e .
```

## Utilisation

Modifier uniquement la fonction `participant_code` dans `competition.py`, puis
brancher une seule carte DDA et lancer :

```powershell
python run_competition.py
```

Le port COM est détecté automatiquement à partir des identifiants USB de la
carte. Aucun numéro de port n'est demandé.

Le lanceur configure automatiquement la carte en mode compétition et sélectionne
une fréquence d'échantillonnage de 5 kHz. Lors de `demarrerLancement()`, l'API
règle le courant initial à 1 A, applique les niveaux par défaut des capteurs et
réveille les quatre ponts.

En mode compétition, les commandes de ponts et de capteurs sont acceptées
uniquement entre `demarrerLancement()` et `arreterLancement()` ou
`annulerLancement()`. Elles sont refusées lorsqu'aucun lancement n'est actif.

Une bobine branchée à l'envers peut être corrigée avant le lancement avec
`carte.inverserDirection(Bobine.Hx)`. Ce réglage est local au programme Python
et ne transmet aucune commande à la carte.

La fermeture normale, une erreur ou `Ctrl+C` déclenchent la procédure d'arrêt
sécuritaire avant de fermer le port série.

Consulter [API.md](API.md) pour les commandes autorisées dans le programme du
participant.
