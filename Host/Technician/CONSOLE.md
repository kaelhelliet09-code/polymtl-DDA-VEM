# Console de commande

Depuis le dossier `Host\Technician`, lancer :

```powershell
python console_carte.py
```

La carte est détectée automatiquement et ouverte en mode débogage. Quelques
exemples :

```python
carte.lireDefauts()
carte.lireEtatSysteme()
carte.reveiller(H1)
carte.reglerCourant(1000, H1)
carte.activer(H1, AVANT)
carte.desactiver(H1)
carte.mettreEnVeille(H1)

# Activer et réarmer les notifications des capteurs
carte.utiliserNiveauxCapteursParDefaut()
carte.deverrouillerCapteurs()

# Attendre un front montant du capteur 1, au maximum 10 secondes
carte.attendreCapteur(C1, MONTANT, 10.0)

# Lire toutes les notifications déjà reçues
carte.prendreNotificationsCapteurs()
```

Les raccourcis disponibles sont `H1`, `H2`, `H3`, `H4`, `TOUTES`, `AVANT` et
`ARRIERE` pour les ponts, ainsi que `C1`, `C2`, `C3`, `C4`, `TOUS_CAPTEURS`,
`MONTANT` et `DESCENDANT` pour les capteurs. Toutes les méthodes de l'API
technicien peuvent aussi être appelées sur l'objet `carte`.

Utiliser `exit()` pour quitter. La fermeture de la console annule un lancement
actif, met tous les ponts en veille et ferme le port série.
