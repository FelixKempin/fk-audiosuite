# Wurzel des Builds. Legt Sprache, Optionen und die Reihenfolge der Unterverzeichnisse fest.
# → LEARNING.md §3 (Kompilieren: Header, Source, Linker, CMake)
cmake_minimum_required(VERSION 3.25)

project(FKAudioSuite
  VERSION 0.7.0 # einzige Quelle der Version (dist/README.md §Version)
  DESCRIPTION "Moderne Open-Source-DAW für Linux und Windows"
  LANGUAGES C CXX)

# Ein Standard für alles: C++23 ohne Compiler-Erweiterungen (ADR 0001).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: C++-Laufzeit statisch (/MT), wie bei eigenständigen Plugins. sfizz setzt das selbst
# (SfizzConfig.cmake); ein Ziel mit /MD daneben ließe sich nicht linken (LNK2038). So braucht
# der Installer auch kein Visual-C++-Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Linux: libstdc++ und libgcc statisch, wie bei eigenständigen Plugins. Gebaut wird mit GCC 13 auf Ubuntu
# 22.04 (Release-Workflow); so hängt das AppImage nur an glibc ≥ 2.35, nicht an einer neueren libstdc++.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_link_options(-static-libstdc++ -static-libgcc)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)

# compile_commands.json für clangd und clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(FK_BUILD_APP "Die Anwendung bauen (braucht JUCE-Systembibliotheken, siehe docs/SETUP.md)" ON)
option(FK_BUILD_PLUGINS "Nur das Plugin-Fundament (core, dsp, juce ohne Tracktion, plugins) bauen; für eigenständige Plugins" OFF)
option(FK_BUILD_TESTS "Tests bauen" ON)
option(FK_ENABLE_SANITIZERS "Address- und UB-Sanitizer einschalten (nur Debug)" OFF)

# MSVC: Quellen sind UTF-8 (Umlaute in Texten), __cplusplus ehrlich melden, große Übersetzungseinheiten
# (Tracktion) zulassen. Ohne /utf-8 liest MSVC die Dateien in der Codepage des Systems.
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8> $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus> $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake") # CURRENT: auch eingebettet (add_subdirectory aus einem anderen Projekt) korrekt
include(FkWarnings)

# Debug/Release-Schalter für ALLE Übersetzungseinheiten gleich setzen. JUCE prüft das beim
# Linken (Fehler "this_will_fail_to_link_if_some_of_your_compile_units_are_built_in_release_mode"):
# Unterschiedliche Schalter ändern Klassenlayouts in JUCE-Headern und wären undefiniertes Verhalten.
add_compile_definitions(
  $<$<CONFIG:Debug>:DEBUG=1>
  $<$<CONFIG:Debug>:_DEBUG=1>
  $<$<NOT:$<CONFIG:Debug>>:NDEBUG=1>)

# Fremdbibliotheken. EXCLUDE_FROM_ALL: nur bauen, was wir tatsächlich verlinken.
if(FK_BUILD_APP OR FK_BUILD_PLUGINS)
  add_subdirectory(external/tracktion_engine/modules/juce EXCLUDE_FROM_ALL)
endif()
if(FK_BUILD_APP)
  add_subdirectory(external/tracktion_engine/modules EXCLUDE_FROM_ALL) # tracktion_core/_engine/_graph
endif()

# sfizz (SFZ-Sampler, BSD-2, ADR 0002): nur die statische Bibliothek, ohne JACK-Programm,
# Renderer, LTO und Tests. Die eigenen Schalter von sfizz liegen alle im Cache (option_ex),
# deshalb als CACHE-Werte setzen, bevor das Unterverzeichnis sie liest.
if(FK_BUILD_APP OR FK_BUILD_PLUGINS)
  set(ENABLE_LTO OFF CACHE BOOL "" FORCE)
  set(SFIZZ_JACK OFF CACHE BOOL "" FORCE)
  set(SFIZZ_RENDER OFF CACHE BOOL "" FORCE)
  set(SFIZZ_SHARED OFF CACHE BOOL "" FORCE)
  set(SFIZZ_TESTS OFF CACHE BOOL "" FORCE)
  set(SFIZZ_DEMOS OFF CACHE BOOL "" FORCE)
  set(SFIZZ_USE_SNDFILE OFF CACHE BOOL "" FORCE)
  # sfizz ist C++17-Code (mit 23 scheitern seine u8-Literale, mit seinem eigenen Standard 14 die
  # Hex-Gleitkomma-Literale). Deshalb für sein Verzeichnis 17 setzen, als normale Variable (erbt
  # das Unterverzeichnis) und im Cache (damit sfizz' eigener Cache-Wert 14 nicht greift), und
  # danach für unseren Code wieder 23.
  set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++-Standard für sfizz; eigener Code nutzt 23")
  set(CMAKE_CXX_STANDARD 17)
  add_subdirectory(external/sfizz EXCLUDE_FROM_ALL)
  set(CMAKE_CXX_STANDARD 23)
  # abseil (in sfizz) schaltet unter MSVC mit AVX - etwa wenn ein Plugin-Projekt alles mit /arch:AVX2 übersetzt -
  # den beschleunigten CRC-Kopierer ein; dessen Code nutzt Designated Initializers, die MSVC erst ab
  # C++20 annimmt (GCC auch in 17). Nur diese eine Bibliothek, sfizz selbst bleibt bei 17.
  if(MSVC AND TARGET absl_crc32c)
    set_target_properties(absl_crc32c PROPERTIES CXX_STANDARD 20)
  endif()
endif()

if(FK_BUILD_TESTS)
  add_subdirectory(external/Catch2 EXCLUDE_FROM_ALL)
  enable_testing()
endif()

# Eigener Code, in Abhängigkeitsreihenfolge (ARCHITECTURE.md §3).
add_subdirectory(src)

if(FK_BUILD_TESTS)
  add_subdirectory(tests)
endif()
