# Die ausführbare Anwendung. JUCE erzeugt daraus unter Linux ein Programm,
# unter Windows eine .exe mit Icon und Versionsinfo.
juce_add_gui_app(FKAudioSuite
  PRODUCT_NAME "FK Audio Suite"
  COMPANY_NAME "FK Online Services"
  VERSION ${PROJECT_VERSION}
  ICON_BIG "${PROJECT_SOURCE_DIR}/resources/icons/fk-audio-suite.png" # Platzhalter, bis es ein Markenlogo gibt
  NEEDS_CURL FALSE
  NEEDS_WEB_BROWSER FALSE)

set(FK_APP_SOURCES
  main.cpp
  Application.h
  Application.cpp
  HangWatchdog.h
  HangWatchdog.cpp)

target_sources(FKAudioSuite PRIVATE ${FK_APP_SOURCES})

# Übersetzungen der DAW (ADR 0010); die des Fundaments bringt fk::plugins mit.
include(FkTranslations)
fk_add_translations(fk_daw_translations NAMESPACE FkDawTranslations FILES ../../resources/lang/de.txt)

# Name und Version aus dem Target, damit es genau eine Quelle dafür gibt.
target_compile_definitions(FKAudioSuite PRIVATE
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:FKAudioSuite,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:FKAudioSuite,JUCE_VERSION>")

# Kein direktes Verlinken von juce::*-Modulen hier: Das macht fk::engine genau einmal
# (siehe src/engine/CMakeLists.txt). Die App bekommt Include-Pfade und Definitionen von dort.
target_link_libraries(FKAudioSuite PRIVATE
  fk::ui
  fk::services
  fk::engine
  fk::core
  fk_daw_translations)

# Nur unsere Dateien: JUCE hängt seine Modul-Quellen in dasselbe Target.
fk_set_warnings_for_sources(FKAudioSuite ${FK_APP_SOURCES})
