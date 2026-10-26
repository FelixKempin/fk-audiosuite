#pragma once

#include <cstdlib>

namespace fk::testing
{

/// Umgebungsvariable für einen Test setzen, auf Linux wie auf Windows. Unter Windows gibt es kein
/// setenv; _putenv_s ändert die Umgebung des Prozesses, die auch juce::SystemStats liest.
inline void setEnvironment (const char* name, const char* value)
{
#if defined(_WIN32)
    ::_putenv_s (name, value);
#else
    ::setenv (name, value, 1);
#endif
}

/// Entfernt die Variable wieder (Windows: ein leerer Wert löscht sie).
inline void unsetEnvironment (const char* name)
{
#if defined(_WIN32)
    ::_putenv_s (name, "");
#else
    ::unsetenv (name);
#endif
}

} // namespace fk::testing
