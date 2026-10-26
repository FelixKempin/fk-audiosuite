#pragma once

namespace fk::ui
{

/// Werkzeug in der Projektzone (UI_CONCEPT.md §2), Tasten wie in Cubase.
enum class Tool
{
    select, ///< Clips auswählen und verschieben (1)
    split,  ///< Clip an der Klickposition teilen (3)
    erase,  ///< Clip löschen (5)
    draw,   ///< Stift: Noten setzen (Key Editor), leere MIDI-Clips zeichnen (Arrangement) (8)
    range,  ///< Bereich über mehrere Spuren aufziehen: löschen, entfernen, Stille, teilen (2)
    glue,   ///< Kleber: Clip mit dem nächsten auf derselben Spur verbinden (4)
    zoom,   ///< Lupe: Klick vergrößert um die Stelle, Alt+Klick verkleinert (6)
    mute    ///< Clip stumm schalten und wieder hörbar machen (7)
};

} // namespace fk::ui
