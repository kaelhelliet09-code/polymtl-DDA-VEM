# Outils du technicien DDA

Ce dossier contient l'API complète de diagnostic ainsi que les procédures de
qualification matérielle. Ces fichiers ne doivent pas être remis aux
participants.

## Installation

Depuis ce dossier :

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -e .
```

Une seule carte DDA doit être branchée. Tous les scripts détectent son port COM
automatiquement.

## Scripts disponibles

```powershell
python console_carte.py
python diagnostic_carte.py
python test_capteurs.py
python test_ponts_h.py
```

- `console_carte.py` ouvre une console interactive pour envoyer directement des
  commandes à la carte;
- `diagnostic_carte.py` lit l'état, les défauts, PMODE et les consignes de
  courant sans alimenter les charges.
- `test_capteurs.py` attend un événement provenant de chacun des quatre
  capteurs.
- `test_ponts_h.py` exécute la qualification alimentée à 1 A puis à 2 A avec la
  barre de résistances.

Lire impérativement [TEST_PONTS_H.md](TEST_PONTS_H.md) avant le test de
puissance et [TEST_CAPTEURS.md](TEST_CAPTEURS.md) avant le test des capteurs.
Des exemples pour la console sont disponibles dans [CONSOLE.md](CONSOLE.md).

## API complète et tests logiciels

Le paquet `dda_host` de ce dossier expose aussi les commandes de diagnostic,
le mode débogage, les défauts, PMODE, les mesures de courant et les rapports de
temporisation. La liste pratique se trouve dans
[API_TECHNICIEN.md](API_TECHNICIEN.md).

Les anciennes commandes d'étalonnage automatique des capteurs restent dans
l'API pour compatibilité, mais le firmware DDA V2 les refuse volontairement.

Pour exécuter les tests sans matériel :

```powershell
python -m unittest discover -s tests -v
```
