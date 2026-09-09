# Vérification des capteurs

Brancher les quatre capteurs, alimenter la carte et lancer :

```powershell
python test_capteurs.py
```

Le script applique les seuils par défaut du firmware et demande de déclencher
chaque capteur. Un événement doit être reçu dans les dix secondes à chaque
étape. Le front reçu est affiché afin d'aider au diagnostic du câblage.

Cette vérification ne réalise pas d'étalonnage automatique. Cette fonction de
l'ancien matériel n'est pas prise en charge par le firmware DDA V2.
