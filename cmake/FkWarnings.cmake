# Compiler-Warnungen für unseren Code. Fremdcode (JUCE, Catch2) bleibt unberührt,
# weil die Funktionen nur auf unsere Targets angewendet werden.
# Warnungen sind Fehler (-Werror): CODING_GUIDELINES.md §13.

# Basis: für alles, was JUCE-Header einbindet. -Wno-ignored-qualifiers, weil JUCE
# an einigen Stellen "const String" als Rückgabetyp verwendet.
# -Wno-missing-field-initializers: GCC 13 meldet sie auch bei designierten Initialisierern
# ({ .title = "x" } lässt die übrigen Felder bewusst auf ihrem Standardwert), was genau die
# Schreibweise ist, die wir für Beschreibungen als Daten nutzen.
set(FK_WARNINGS_GCC_LIKE
  -Wall -Wextra -Werror
  -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual
  -Wno-ignored-qualifiers -Wno-missing-field-initializers)

# Streng: zusätzlich für core und dsp, die keine JUCE-Header einbinden.
set(FK_STRICT_WARNINGS_GCC_LIKE
  -Wpedantic -Wconversion -Wsign-conversion -Wold-style-cast
  -Wcast-align -Wdouble-promotion -Wformat=2 -Wnull-dereference)

# MSVC: /W4 ohne /WX, bis der erste Windows-Build sauber ist (CI meldet die Warnungen trotzdem).
# Danach FK_MSVC_WARNINGS_AS_ERRORS einschalten, wie -Werror unter Linux.
option(FK_MSVC_WARNINGS_AS_ERRORS "Warnungen unter MSVC als Fehler (/WX)" OFF)
set(FK_WARNINGS_MSVC /W4 /permissive-)

if(FK_MSVC_WARNINGS_AS_ERRORS)
  list(APPEND FK_WARNINGS_MSVC /WX)
endif()

function(fk_set_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:${FK_WARNINGS_GCC_LIKE}>
    $<$<CXX_COMPILER_ID:MSVC>:${FK_WARNINGS_MSVC}>)
  fk_set_sanitizers(${target})
endfunction()

function(fk_set_strict_warnings target)
  fk_set_warnings(${target})
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:${FK_STRICT_WARNINGS_GCC_LIKE}>)
endfunction()

# Für Targets, in die JUCE seine eigenen Modul-Quellen einhängt (juce_add_gui_app):
# Warnungen nur auf die angegebenen eigenen Dateien, damit Fremdcode (auch C-Dateien
# wie Sheenbidi) nicht mit unseren C++-Flags und -Werror gebaut wird.
function(fk_set_warnings_for_sources target)
  set(sources ${ARGN})
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set_source_files_properties(${sources} PROPERTIES COMPILE_OPTIONS "${FK_WARNINGS_GCC_LIKE}")
  elseif(MSVC)
    set_source_files_properties(${sources} PROPERTIES COMPILE_OPTIONS "${FK_WARNINGS_MSVC}")
  endif()
  fk_set_sanitizers(${target})
endfunction()

function(fk_set_sanitizers target)
  if(FK_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()
