# fk::plugins – Plugin-Fundament (Ausgabe, Übersetzung, UI-Kit) und die kleinen Effekte/Instrumente
# der DAW als juce::AudioProcessor. Kennt JUCE und fk::dsp, aber nie Tracktion, damit dieselben
# Klassen auch in eigenständigen Plugins laufen (FK_BUILD_PLUGINS).

set(FK_PLUGIN_SOURCES
  fk/plugins/InternalPlugin.h
  fk/plugins/InternalPlugin.cpp
  fk/plugins/PluginCatalog.h
  fk/plugins/PluginCatalog.cpp
  fk/plugins/edition/Edition.h
  fk/plugins/edition/FeatureGate.h
  fk/plugins/edition/FeatureGate.cpp
  fk/plugins/i18n/Language.h
  fk/plugins/i18n/Language.cpp
  fk/plugins/i18n/Translator.h
  fk/plugins/i18n/Translator.cpp
  fk/plugins/i18n/Text.h
  fk/plugins/i18n/Text.cpp
  fk/plugins/i18n/SharedSettings.h
  fk/plugins/i18n/SharedSettings.cpp
  fk/plugins/i18n/LanguageSetting.h
  fk/plugins/i18n/LanguageSetting.cpp
  fk/plugins/i18n/FoundationTranslations.h
  fk/plugins/i18n/FoundationTranslations.cpp
  fk/plugins/ui/PluginTheme.h
  fk/plugins/ui/PluginLookAndFeel.h
  fk/plugins/ui/PluginLookAndFeel.cpp
  fk/plugins/ui/Knob.h
  fk/plugins/ui/Knob.cpp
  fk/plugins/ui/Switch.h
  fk/plugins/ui/Switch.cpp
  fk/plugins/ui/Choice.h
  fk/plugins/ui/Choice.cpp
  fk/plugins/ui/Lcd.h
  fk/plugins/ui/Lcd.cpp
  fk/plugins/ui/Meter.h
  fk/plugins/ui/Meter.cpp
  fk/plugins/ui/ModulePanel.h
  fk/plugins/ui/ModulePanel.cpp
  fk/plugins/ui/PanelDescription.h
  fk/plugins/ui/PanelView.h
  fk/plugins/ui/PanelView.cpp
  fk/plugins/ui/PanelEditor.h
  fk/plugins/ui/PanelEditor.cpp
  fk/plugins/ui/StatusText.h
  fk/plugins/ui/StatusText.cpp
  fk/plugins/ui/FrequencyScale.h
  fk/plugins/ui/SpectrumView.h
  fk/plugins/ui/SpectrumView.cpp
  fk/plugins/ui/CurveView.h
  fk/plugins/ui/CurveView.cpp
  fk/plugins/ui/TransferView.h
  fk/plugins/ui/TransferView.cpp
  fk/plugins/ui/BandHandles.h
  fk/plugins/ui/BandHandles.cpp
  fk/plugins/analysis/SpectrumAnalyser.h
  fk/plugins/analysis/SpectrumAnalyser.cpp
  fk/plugins/GainProcessor.h
  fk/plugins/GainProcessor.cpp
  fk/plugins/EqProcessor.h
  fk/plugins/EqProcessor.cpp
  fk/plugins/EqView.h
  fk/plugins/EqView.cpp
  fk/plugins/CompressorProcessor.h
  fk/plugins/CompressorProcessor.cpp
  fk/plugins/LimiterProcessor.h
  fk/plugins/LimiterProcessor.cpp
  fk/plugins/ReverbProcessor.h
  fk/plugins/ReverbProcessor.cpp
  fk/plugins/DelayProcessor.h
  fk/plugins/DelayProcessor.cpp
  fk/plugins/GateProcessor.h
  fk/plugins/GateProcessor.cpp
  fk/plugins/TrimProcessor.h
  fk/plugins/TrimProcessor.cpp
  fk/plugins/DistortionProcessor.h
  fk/plugins/DistortionProcessor.cpp
  fk/plugins/SubtractiveSynthProcessor.h
  fk/plugins/SubtractiveSynthProcessor.cpp
  fk/plugins/SamplerProcessor.h
  fk/plugins/SamplerProcessor.cpp
  fk/plugins/midi/MidiEffects.h
  fk/plugins/midi/MidiEffects.cpp
  fk/plugins/MidiEffectProcessors.h
  fk/plugins/MidiEffectProcessors.cpp)

# Übersetzungen des Fundaments (ADR 0010), eingebettet wie Content: FoundationTranslations.cpp liest
# sie über FkFoundationTranslations.h. Eigenes Target, weil JUCE Binärdaten als Bibliothek erzeugt.
include(FkTranslations)
fk_add_translations(fk_foundation_translations NAMESPACE FkFoundationTranslations FILES lang/de.txt)

if(FK_BUILD_APP)
  # In der DAW: statische Bibliothek gegen fk::juce (JUCE genau einmal kompiliert, §7a).
  add_library(fk_plugins STATIC ${FK_PLUGIN_SOURCES})
  add_library(fk::plugins ALIAS fk_plugins)

  target_include_directories(fk_plugins PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(fk_plugins PUBLIC fk::core fk::dsp fk::juce sfizz::static fk_foundation_translations)

  fk_set_warnings_for_sources(fk_plugins ${FK_PLUGIN_SOURCES})
else()
  # Ohne DAW (FK_BUILD_PLUGINS, für eigenständige Plugins): Die Quellen wandern wie bei JUCE-Modulen in
  # das Target, das uns verlinkt (ein VST3, ein Standalone, die Tests). So kompiliert jedes
  # Plugin JUCE und das Fundament genau einmal, und juce_add_plugin setzt seine eigenen
  # Schalter (JUCE_STANDALONE_APPLICATION, Splash) ohne Konflikt mit fk::juce.
  set(FK_PLUGIN_SOURCES_ABSOLUTE "")
  foreach(source IN LISTS FK_PLUGIN_SOURCES)
    list(APPEND FK_PLUGIN_SOURCES_ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
  endforeach()

  add_library(fk_plugins INTERFACE)
  add_library(fk::plugins ALIAS fk_plugins)

  target_sources(fk_plugins INTERFACE ${FK_PLUGIN_SOURCES_ABSOLUTE})
  target_include_directories(fk_plugins INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(fk_plugins INTERFACE
    fk::core
    fk::dsp
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    sfizz::static
    fk_foundation_translations)
  target_compile_definitions(fk_plugins INTERFACE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_VST3_CAN_REPLACE_VST2=0)

  # Wer uns verlinkt, ruft fk_set_warnings_for_sources(<target> ${FK_PLUGIN_SOURCES_ABSOLUTE}),
  # weil Quelldatei-Eigenschaften nicht über Verzeichnisse hinweg gelten.
  set_target_properties(fk_plugins PROPERTIES FK_SOURCES "${FK_PLUGIN_SOURCES_ABSOLUTE}")
endif()
