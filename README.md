# FK Audio Suite

Eine moderne, quelloffene DAW (Digital Audio Workstation) für Linux und Windows.
Oberfläche und Workflow orientieren sich an Cubase, der Audio-Kern basiert auf
JUCE und Tracktion Engine.

Das Projekt ist gleichzeitig ein Lernprojekt: Der Code soll ein durchgehend
sauberer, konsistenter C++-Stil sein, den man als Java-Entwickler Schritt für
Schritt nachvollziehen kann.

## Status

Öffentliche Beta (0.7.0). Was die DAW kann, steht in [CHANGELOG.md](CHANGELOG.md), was als Nächstes kommt, in [docs/ROADMAP.md](docs/ROADMAP.md). Die Anwendung braucht die Systembibliotheken aus [docs/SETUP.md](docs/SETUP.md).

Bauen: siehe [docs/SETUP.md](docs/SETUP.md). Lernen: [docs/LEARNING.md](docs/LEARNING.md).

## Dokumente

| Dokument | Inhalt |
|---|---|
| [docs/FEATURES.md](docs/FEATURES.md) | Feature-Analyse mit Priorisierung |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Schichten, Module, Threading, Datenfluss |
| [docs/UI_CONCEPT.md](docs/UI_CONCEPT.md) | Fensteraufbau, Zonen, Designsystem |
| [docs/CODING_GUIDELINES.md](docs/CODING_GUIDELINES.md) | Der eine Stil: Regeln für allen Code |
| [docs/LEARNING.md](docs/LEARNING.md) | C++ für Java-Entwickler, anhand des Projekts |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Meilensteine |
| [docs/SETUP.md](docs/SETUP.md) | Entwicklungsumgebung einrichten |
| [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md) | Fremdbibliotheken und Lizenzen |
| [docs/adr/](docs/adr/) | Architekturentscheidungen (ADRs) |

## Lizenz

GPLv3 (siehe [ADR 0003](docs/adr/0003-lizenz-gplv3.md)).
