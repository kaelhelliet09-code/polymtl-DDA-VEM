# DDA Eval Board — capture de vitesse

Ce dossier est un projet autonome pour une carte consacrée uniquement à la
capture de vitesse. Il ne dépend d'aucun code Competition ou Technician et ne
modifie pas le firmware de la carte DDA normale.

## Fonctionnement

- Le script trouve automatiquement l'Eval Board avec `VID 0x0483` et
  `PID 0x5741`. Une carte DDA normale (`PID 0x5740`) peut donc rester branchée
  en même temps.
- La commande `0x01` démarre l'acquisition et la commande `0x02` l'arrête.
- PC6 / TIM2_CH3 est le premier capteur traversé.
- PC7 / TIM2_CH4 est le second capteur traversé.
- TIM2 compte à 64 MHz et la distance entre capteurs est de 50 mm.
- Chaque événement USB contient `t1`, `t2` et la vitesse en m/s dans une trame
  binaire de 13 octets: `0x56 + uint32 t1 + uint32 t2 + float32 vitesse`, en
  little-endian.

Le firmware initialise seulement GPIO, TIM2 et USB. Les quatre ponts en H
restent endormis: toutes leurs entrées et leurs broches `nSLEEP` sont forcées à
0. Les ADC, INA226, DAC et fonctions de lancement ne sont pas initialisés.
`SENSOR_ENA` reste à 1 pour alimenter les capteurs.

## Compiler le firmware

Depuis la racine `DDA_V2`:

```powershell
cmake -S EvalBoard/Firmware -B EvalBoard/Firmware/build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build EvalBoard/Firmware/build
```

Programmer ensuite
`EvalBoard/Firmware/build/DDA_EvalBoard.elf` avec ST-LINK ou
STM32CubeProgrammer. Les fichiers `.hex` et `.bin` sont générés au même endroit.

## Lancer la capture

```powershell
python -m pip install -r EvalBoard/Host/requirements.txt
python EvalBoard/Host/read_velocity.py
```

Le script envoie START dès l'ouverture du port, affiche chaque paire de
timestamps et sa vitesse, puis envoie STOP avec `Ctrl+C`.
