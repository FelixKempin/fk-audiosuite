# fk::engine – Anbindung an Tracktion Engine und JUCE-Audio.
add_library(fk_engine STATIC
  fk/engine/EngineContext.h
  fk/engine/EngineContext.cpp
  fk/engine/FkEngineBehaviour.h
  fk/engine/FkEngineBehaviour.cpp
  fk/engine/ExpressionMap.h
  fk/engine/ExpressionMap.cpp
  fk/engine/FkUIBehaviour.cpp
  fk/engine/TestToneSource.h
  fk/engine/TestToneSource.cpp)

add_library(fk::engine ALIAS fk_engine)

target_include_directories(fk_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# JUCE/Tracktion kommen über fk::juce (genau einmal kompiliert); eigene Plugins werden
# hier bei der Engine registriert, deshalb hängt engine von plugins ab, nie umgekehrt.
target_link_libraries(fk_engine PUBLIC
  fk::core
  fk::dsp
  fk::plugins
  fk::juce)

fk_set_warnings_for_sources(fk_engine
  fk/engine/EngineContext.cpp
  fk/engine/ExpressionMap.cpp
  fk/engine/FkEngineBehaviour.cpp
  fk/engine/FkUIBehaviour.cpp
  fk/engine/TestToneSource.cpp)
