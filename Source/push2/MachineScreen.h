#pragma once

#include <juce_graphics/juce_graphics.h>

namespace mbstudio
{

// Il display del Push, 960x160, disegnato come la serigrafia di un pannello:
// fondo nero, scritte bianche maiuscole, una riga di campi in basso.
// `lines` sono i campi, scritti come "ETICHETTA|VALORE"; al massimo otto, uno
// per ogni rotativo. La prima riga, se comincia per '#', e' il titolo grande.
juce::Image renderMachineScreen (const juce::StringArray& lines,
                                 const juce::String& firstBank,
                                 const juce::String& lastBank,
                                 juce::Typeface::Ptr font);

} // namespace mbstudio
