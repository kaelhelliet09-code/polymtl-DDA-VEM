# Outils hôte de la carte DDA

Le logiciel hôte est séparé en deux dossiers indépendants :

- [Competition](Competition/README.md) contient le programme remis aux
  participants et une API limitée aux fonctions du concours;
- [Technician](Technician/README.md) contient l'API complète, les diagnostics
  et les procédures de qualification matérielle.

Chaque dossier possède son propre environnement Python, ses dépendances et sa
documentation. Il faut exécuter les commandes depuis le dossier correspondant.

Dans les deux cas, le port COM est détecté automatiquement lorsqu'une seule
carte DDA est branchée.
