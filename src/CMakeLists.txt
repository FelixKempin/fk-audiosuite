# Module in Abhängigkeitsreihenfolge: Jedes darf nur die vor ihm stehenden benutzen.
add_subdirectory(core)
add_subdirectory(dsp)

# Das Plugin-Fundament baut auch ohne DAW (FK_BUILD_PLUGINS, ADR 0008) für eigenständige Plugins.
if(FK_BUILD_APP)
  add_subdirectory(juce)
endif()
if(FK_BUILD_APP OR FK_BUILD_PLUGINS)
  add_subdirectory(plugins)
endif()

if(FK_BUILD_APP)
  add_subdirectory(engine)
  add_subdirectory(services)
  add_subdirectory(ui)
  add_subdirectory(app)
endif()
