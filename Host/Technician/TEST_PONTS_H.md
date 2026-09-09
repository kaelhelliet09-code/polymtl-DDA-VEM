# Qualification des ponts en H

## Matériel requis

- une carte DDA avec le firmware courant;
- une alimentation de laboratoire protégée et limitée en courant;
- la barre comprenant une résistance de `6,9 Ω` adaptée à chaque pont;
- le refroidissement et les instruments normalement utilisés au banc.

Chaque résistance doit être dimensionnée pour supporter la puissance du test.
Ne jamais effectuer cette procédure avec les sorties ouvertes, en court-circuit
ou avec une charge de valeur inconnue.

## Procédure

1. Couper l'alimentation de puissance.
2. Relier une résistance de `6,9 Ω` à chaque sortie H1, H2, H3 et H4.
3. Vérifier le câblage, la limitation de courant et le refroidissement.
4. Alimenter la carte et la relier par USB.
5. Si un défaut précédent est mémorisé, réinitialiser la carte.
6. Depuis le dossier `Technician`, lancer :

```powershell
python test_ponts_h.py
```

Le script détecte automatiquement la carte. Après confirmation, il commande
H1, H2, H3, H4 puis les quatre ponts ensemble, d'abord à 1 A puis à 2 A. Chaque
activation dure 0,5 seconde et est suivie d'une commande d'arrêt.

## Critères existants

- À 1 A, aucun défaut ne doit être présent.
- À 2 A, l'étape avec les quatre ponts doit provoquer la protection prévue.
- Le graphique final doit montrer l'activation séquentielle, des courants
  plausibles et la coupure provoquée par la protection.

Le test échoue si un défaut apparaît à 1 A ou si aucun défaut n'est détecté à
2 A. Il ne réarme pas automatiquement un défaut matériel mémorisé.
