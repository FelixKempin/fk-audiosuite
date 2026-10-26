# ADR 0001: Sprache C++23 mit eingeschränktem Feature-Satz

Datum: 2026-10-08 · Status: angenommen

## Kontext
Der Entwickler kennt Java, kein C++. Erste Wahl war Java. Echtzeit-Audio (keine GC-Pausen, keine Allokation im Audio-Thread) und Plugin-Hosting (VST3/CLAP sind native C/C++-Schnittstellen) schließen einen Java-Kern aus. Rust hat kein DAW-Engine-Ökosystem. Die gesamte Audio-Welt (JUCE, Tracktion, Plugin-SDKs, Community) ist C++.

## Entscheidung
C++ nach Standard C++23, Compiler GCC ≥ 13 und MSVC ≥ 19.38. Von C++23 nutzen wir gezielt `std::expected`; sonst bleibt der Feature-Satz auf das beschränkt, was in `CODING_GUIDELINES.md` §1 erlaubt ist. Keine Module, keine Coroutinen, keine Concepts, bis ein Bedarf dokumentiert ist.

## Folgen
- Der Entwickler lernt C++ am Projekt; `LEARNING.md` begleitet das.
- Die Feature-Beschränkung hält den Code einheitlich und lernbar.
- Clang unter Linux ist ebenfalls möglich (≥ 17), ist aber nicht der Referenz-Compiler.
