# fk::core – Grundbausteine ohne Framework-Abhängigkeit: Fehler, Ergebnisse.
add_library(fk_core STATIC
  fk/core/Error.h
  fk/core/Error.cpp
  fk/core/Result.h)

add_library(fk::core ALIAS fk_core)

# Versionsinfo des Builds (fk/core/BuildInfo.h). Git-Stand und Datum beim Konfigurieren; ein Build aus
# einer Kopie ohne .git bekommt sie über -DFK_GIT_SHA.
set(FK_RELEASE_CHANNEL "dev" CACHE STRING "Auslieferungskanal: dev, beta oder stable")
set(FK_UPDATE_URL "" CACHE STRING "Adresse der latest.json für Update-Hinweise; leer = aus")
set(FK_WEBSITE_URL "https://fk-audiosuite.com" CACHE STRING "Website: Download, Preise und Aktivierung der Plugins (Hilfe-Menü)")
set(FK_GIT_SHA "" CACHE STRING "Git-Stand; leer = beim Konfigurieren ermitteln")

if(FK_GIT_SHA STREQUAL "")
  execute_process(COMMAND git rev-parse --short HEAD
                  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                  OUTPUT_VARIABLE FK_GIT_SHA_DETECTED
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)
  set(FK_GIT_SHA "${FK_GIT_SHA_DETECTED}")
endif()

if(FK_GIT_SHA STREQUAL "")
  set(FK_GIT_SHA "unbekannt")
endif()

string(TIMESTAMP FK_BUILD_DATE "%Y-%m-%d" UTC)

if(WIN32)
  set(FK_PLATFORM_NAME "windows-x64")
else()
  set(FK_PLATFORM_NAME "linux-x64")
endif()

configure_file(fk/core/BuildInfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/fk/core/BuildInfo.h @ONLY)

# PUBLIC: Wer fk::core verlinkt, bekommt den Include-Pfad mit, damit
# #include "fk/core/Result.h" überall gleich aussieht.
target_include_directories(fk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/generated)
fk_set_strict_warnings(fk_core)
