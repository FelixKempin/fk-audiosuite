# fk::juce – kompiliert JUCE und Tracktion Engine GENAU EINMAL. Nur für die DAW (FK_BUILD_APP);
# ohne DAW bringt fk::plugins JUCE als Quellen mit (src/plugins/CMakeLists.txt).
#
# JUCE-/Tracktion-Module sind Interface-Bibliotheken: Ihre Quellen werden in das Target
# kompiliert, das sie verlinkt. Damit das nur hier passiert, verlinkt dieses Target die
# Module PRIVATE und reicht Include-Pfade und Compile-Definitionen INTERFACE weiter.
# Alle anderen Module verlinken fk::juce (direkt oder indirekt), nie juce::* selbst
# (ARCHITECTURE.md §7a).
add_library(fk_juce STATIC
  fk/juce/JuceLink.cpp)

add_library(fk::juce ALIAS fk_juce)

target_link_libraries(fk_juce
  PUBLIC
    # Nur Compile-Definitionen, keine Quellen: darf PUBLIC sein und muss es, damit jede
    # Übersetzungseinheit JUCE-Header mit denselben Schaltern sieht.
    juce::juce_recommended_config_flags
  PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_cryptography
    juce::juce_recommended_lto_flags)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(fk_juce PUBLIC atomic)
endif()

# Modul-Konfiguration für alle, die JUCE-Header einbinden.
target_compile_definitions(fk_juce PUBLIC
  JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
  JUCE_STANDALONE_APPLICATION=1
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  # JACK und ALSA nur unter Linux: unter Windows bindet JUCE mit JUCE_JACK=1
  # jack/jack.h ein, das es dort nicht gibt.
  $<$<PLATFORM_ID:Linux>:JUCE_JACK=1>
  $<$<PLATFORM_ID:Linux>:JUCE_ALSA=1>
  # ASIO unter Windows: JUCE bringt die ASIO-Header von Steinberg mit, für die DAW unter GPLv3
  # (ADR 0003). WASAPI und DirectSound sind ohnehin an.
  $<$<PLATFORM_ID:Windows>:JUCE_ASIO=1>
  JUCE_PLUGINHOST_VST3=1
  JUCE_PLUGINHOST_LV2=1
  JUCE_PLUGINHOST_LADSPA=0
  JUCE_MODAL_LOOPS_PERMITTED=1
  JUCE_STRICT_REFCOUNTEDPOINTER=1
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_USE_MP3AUDIOFORMAT=1 # MP3 lesen (JUCE-Decoder, GPL-verträglich); Import per Drag-and-drop
  JUCE_USE_LAME_AUDIO_FORMAT=1 # MP3 schreiben: Tracktions LAME-Anbindung
  TRACKTION_ENABLE_LIBLAME=1 # libmp3lame (LGPL) wird zur Laufzeit geladen, nicht gelinkt; fehlt sie, ist MP3 aus
  TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
  TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1 # Signalsmith Stretch (MIT), Standard für Time-Stretch und Pitch-Shift
  TRACKTION_UNIT_TESTS=0)

target_include_directories(fk_juce INTERFACE
  $<TARGET_PROPERTY:fk_juce,INCLUDE_DIRECTORIES>)
target_compile_definitions(fk_juce INTERFACE
  $<FILTER:$<TARGET_PROPERTY:fk_juce,COMPILE_DEFINITIONS>,INCLUDE,^JUCE_MODULE_AVAILABLE_>)
