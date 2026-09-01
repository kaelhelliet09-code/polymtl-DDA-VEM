# DDA Python organizer

The host package provides one supported API, `CompetitionBoard`, for the
firmware's three-byte USB service protocol. A receiver thread handles command
responses, fault and sensor notifications, and CRC-checked launch data.

## Install

Python 3.10 or newer is required.

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -e .
```

The connection auto-detects USB VID `0x0483`, PID `0x5740` when exactly one
board is attached. Otherwise pass `--port COM7`.

## Technician tests

Run the supported coil qualification procedure:

```powershell
python run_competition.py --port COM7 --technician-test coil
```

The coil procedure prompts for the resistor bar, records the 1 A and 2 A bridge
sequences, prints both fault checks, and displays the final launch graph. See
the root README for the full technician checklist.

The V1 automatic sensor-calibration API remains import-compatible for older
host programs, but this V2 firmware rejects it: IR-LED current is no longer a
DAC-controlled or stored calibration value.

## Organizer and participant use

La documentation française complète de l'API des participants est disponible
dans [API_PARTICIPANTS.md](API_PARTICIPANTS.md).

Run participant code in debug mode:

```powershell
python run_competition.py --port COM7
```

Use `--competition` to enable the normal during-launch command lockout.
Participants edit only `examples/competition.py`; the runner owns connection,
initial setup, safe interruption handling, LaunchData reception, and plotting.

| Mode | During-launch participant commands | Timing output |
| --- | --- | --- |
| Debug, default | Coil and sensor commands are accepted if safety permits | Console statistics and latency plot |
| Competition, `--competition` | Coil and sensor commands are rejected | No request timing data or plot |

Both modes retain fault handling and the physical/software safety gates. Both
also return current, power, sensor, velocity, and launch-reference data.
Debug mode additionally reports count, average, maximum, median, and p95 for
request and sensor paths. One-way host/device figures are clock-correlated
estimates; request round-trip time is measured directly.

```python
from dda_host import Bobine, Capteur, CarteCompetition, Direction

with CarteCompetition.connecter("COM7") as carte:
    carte.reglerFrequenceEchantillonnage(5000)
    carte.reglerCourant(1000, Bobine.H1)
    assert carte.lireCourant(Bobine.H1) == 1000
    carte.reveiller(Bobine.TOUTES)
    carte.demarrerLancement()
    carte.activer(Bobine.H1, Direction.AVANT)
    evenement = carte.attendreCapteur(Capteur.CAPTEUR_1, delai_s=2.0)
    carte.desactiver(Bobine.H1)
    resultat = carte.arreterLancement()
```

Programmatic users can inspect or format the same debug report:

```python
from dda_host import formaterRapportTemporisation

if resultat.rapport_temporisation is not None:
    print(formaterRapportTemporisation(resultat.rapport_temporisation))
    for sample in resultat.rapport_temporisation.requests:
        print(sample.service, sample.command, sample.round_trip_us)
```

The detailed [request and sensor latency architecture](../docs/architecture/request-timing.md)
documents the firmware snapshots, host capture boundaries, clock conversion,
matching rules, statistics, and limitations.

The context manager attempts to abort an active launch and sleep all bridges
before closing the serial port.

Current limits may be set globally or per bridge with `setCurrent`; use
`getCurrent` for individual readback. `setPmode`/`getPmode` control the shared
DRV8874 mode. A PMODE change is accepted only after every bridge has been put
to sleep.

## Test without hardware

```powershell
python -m unittest discover -s tests -v
```
