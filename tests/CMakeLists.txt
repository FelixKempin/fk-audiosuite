# Alle Tests in einer ausführbaren Datei; Catch2 findet die TEST_CASEs selbst.
set(FK_TEST_SOURCES
  core/ResultTest.cpp
  dsp/TestSignals.h
  dsp/SineOscillatorTest.cpp
  dsp/DecibelsTest.cpp
  dsp/BiquadTest.cpp
  dsp/DynamicsTest.cpp
  dsp/WaveshaperTest.cpp
  dsp/SynthBlocksTest.cpp
  dsp/NoiseGateTest.cpp
  dsp/TempoEstimatorTest.cpp
  dsp/ReverbDelayTest.cpp
  dsp/FractionalDelayLineTest.cpp)

set(FK_TEST_LIBS fk::core fk::dsp)

# Plugin-Tests brauchen JUCE, Service-Tests zusätzlich die Engine.
if(FK_BUILD_APP OR FK_BUILD_PLUGINS)
  list(APPEND FK_TEST_SOURCES
    plugins/SynthTest.cpp
    plugins/SpaceAndGateTest.cpp
    plugins/SamplerTest.cpp
    plugins/MidiEffectsTest.cpp
    plugins/FeatureGateTest.cpp
    plugins/I18nTest.cpp
    i18n/TranslationAudit.h
    i18n/TranslationAudit.cpp
    i18n/TranslationAuditTest.cpp
    i18n/TranslationCoverageTest.cpp
    plugins/PanelEditorTest.cpp
    plugins/SpectrumAnalyserTest.cpp
    plugins/EqViewTest.cpp)
  list(APPEND FK_TEST_LIBS fk::plugins)
endif()

if(FK_BUILD_APP)
  list(APPEND FK_TEST_SOURCES
    services/TestEnvironment.h
    services/ProjectServiceTest.cpp
    services/ProjectSetupTest.cpp
    services/TrackOrganisationTest.cpp
    services/MarkerServiceTest.cpp
    services/TransportA3Test.cpp
    services/ClipProcessingTest.cpp
    services/StretchTest.cpp
    services/SidechainTest.cpp
    services/TransportServiceTest.cpp
    services/TimelineServiceTest.cpp
    services/ClipServiceTest.cpp
    services/PluginServiceTest.cpp
    services/InstrumentTrackTest.cpp
    services/RoutingTest.cpp
    services/ExportA7Test.cpp
    services/RackServiceTest.cpp
    services/ContentServiceTest.cpp
    services/MidiClipServiceTest.cpp
    services/MidiRecordingTest.cpp
    services/ExportServiceTest.cpp
    services/SendServiceTest.cpp
    services/AutomationServiceTest.cpp
    services/SignatureServiceTest.cpp
    services/PreviewServiceTest.cpp
    services/MidiLearnServiceTest.cpp
    services/BakeServiceTest.cpp
    services/NotePreviewServiceTest.cpp
    ui/ArrangeViewSnapshotTest.cpp
    ui/ArrangeViewDropTest.cpp
    ui/ProjectComponentLayoutTest.cpp
    ui/SampleEditorSnapshotTest.cpp
    ui/ArrangeViewClipEditTest.cpp
    ui/NoteDragTest.cpp
    ui/MediaBrowserSnapshotTest.cpp
    ui/KeyEditorSnapshotTest.cpp
    ui/OperationA8Test.cpp
    ui/DrumEditorTest.cpp
    ui/MidiToolsTest.cpp
    ui/ArticulationTest.cpp
    manual/ManualShot.h
    manual/KeyEditorScenes.cpp
    manual/ProjectWindowScene.h
    manual/ProjectWindowScenes.cpp)
  list(APPEND FK_TEST_LIBS fk::services fk::engine fk::ui)
endif()

add_executable(fk_tests ${FK_TEST_SOURCES})
# Gemeinsame Test-Helfer (support/), auch für eigenständige Plugins.
target_include_directories(fk_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(fk_tests PRIVATE ${FK_TEST_LIBS} Catch2::Catch2WithMain)

# Übersetzungsprüfung (tests/i18n): liest die Quellen dieses Repos; die DAW-Tabelle nur mit DAW.
target_compile_definitions(fk_tests PRIVATE
  FK_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  FK_TEST_DAW_TRANSLATIONS=$<BOOL:${FK_BUILD_APP}>)

# core/dsp-Tests streng; der Service-Test bindet JUCE-Header ein und bekommt die Basis-Warnungen.
fk_set_warnings_for_sources(fk_tests ${FK_TEST_SOURCES})

# Ohne DAW bringt fk::plugins seine Quellen mit (INTERFACE); Warnungen und JUCE-Schalter dann hier.
if(FK_BUILD_PLUGINS AND NOT FK_BUILD_APP)
  get_target_property(FK_PLUGIN_SOURCES_ABSOLUTE fk_plugins FK_SOURCES)
  fk_set_warnings_for_sources(fk_tests ${FK_PLUGIN_SOURCES_ABSOLUTE})
  target_compile_definitions(fk_tests PRIVATE JUCE_STANDALONE_APPLICATION=1 $<$<PLATFORM_ID:Linux>:JUCE_JACK=1>)
endif()

# Registriert jeden TEST_CASE einzeln bei CTest.
list(APPEND CMAKE_MODULE_PATH "${FKAudioSuite_SOURCE_DIR}/external/Catch2/extras")
include(Catch)
catch_discover_tests(fk_tests)
