# fk::dsp – reine Signalverarbeitung. Kennt weder JUCE noch Tracktion (ARCHITECTURE.md §3).
add_library(fk_dsp STATIC
  fk/dsp/units/Decibels.h
  fk/dsp/generators/SineOscillator.h
  fk/dsp/generators/SineOscillator.cpp
  fk/dsp/filters/Biquad.h
  fk/dsp/dynamics/EnvelopeFollower.h
  fk/dsp/dynamics/EnvelopeFollower.cpp
  fk/dsp/dynamics/GainComputer.h
  fk/dsp/dynamics/GainComputer.cpp
  fk/dsp/dynamics/Compressor.h
  fk/dsp/dynamics/Compressor.cpp
  fk/dsp/delay/DelayLine.h
  fk/dsp/dynamics/Limiter.h
  fk/dsp/dynamics/Limiter.cpp
  fk/dsp/nonlinear/Waveshaper.h
  fk/dsp/nonlinear/Waveshaper.cpp
  fk/dsp/generators/Oscillator.h
  fk/dsp/generators/Oscillator.cpp
  fk/dsp/envelopes/Adsr.h
  fk/dsp/envelopes/Adsr.cpp
  fk/dsp/filters/StateVariableFilter.h
  fk/dsp/filters/StateVariableFilter.cpp
  fk/dsp/filters/OnePole.h
  fk/dsp/dynamics/NoiseGate.h
  fk/dsp/dynamics/NoiseGate.cpp
  fk/dsp/analysis/TempoEstimator.h
  fk/dsp/analysis/TempoEstimator.cpp
  fk/dsp/delay/StereoDelay.h
  fk/dsp/delay/StereoDelay.cpp
  fk/dsp/reverb/FdnReverb.h
  fk/dsp/reverb/FdnReverb.cpp
  fk/dsp/delay/FractionalDelayLine.h
  fk/dsp/delay/FractionalDelayLine.cpp)

add_library(fk::dsp ALIAS fk_dsp)

target_include_directories(fk_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fk_dsp PUBLIC fk::core)
fk_set_strict_warnings(fk_dsp)
