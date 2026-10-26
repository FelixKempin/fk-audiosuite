# fk_add_translations(<target> NAMESPACE <ns> FILES <lang/de.txt ...>)
#
# Bettet Übersetzungstabellen (juce::LocalisedStrings-Format, ADR 0010) als Binärdaten ein. Der
# Bezeichner kommt aus dem Dateinamen: lang/de.txt wird <ns>::de_txt mit <ns>::de_txtSize. DAW und
# Eigenständige Plugins nutzen dieselbe Funktion, damit jede Tabelle gleich eingebunden wird.
function(fk_add_translations target)
  cmake_parse_arguments(ARG "" "NAMESPACE" "FILES" ${ARGN})
  set(absolute "")
  foreach(file IN LISTS ARG_FILES)
    get_filename_component(path "${file}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    list(APPEND absolute "${path}")
  endforeach()
  juce_add_binary_data(${target}
    HEADER_NAME ${ARG_NAMESPACE}.h
    NAMESPACE ${ARG_NAMESPACE}
    SOURCES ${absolute})
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()
