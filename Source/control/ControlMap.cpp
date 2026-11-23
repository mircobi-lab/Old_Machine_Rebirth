#include "ControlMap.h"

namespace control
{

namespace
{
std::vector<Control> makeButtons()
{
    return {
        // --- sopra il display: le otto pagine
        { 102, Command::pageSample,   0, "CAMPIONA", true },
        { 103, Command::pageChop,     1, "FETTE",    true },
        { 104, Command::pageSound,    2, "SUONO",    true },
        { 105, Command::pageSequence, 3, "SEQUENZA", true },
        { 106, Command::pageStep,     4, "PASSO",    true },
        { 107, Command::pageSong,     5, "BRANO",    true },
        { 108, Command::pageMix,      6, "MIX",      true },
        { 109, Command::pageDisk,     7, "DISCO",    true },

        // --- sotto il display: i tasti funzione della pagina aperta
        { 20, Command::softKey, 0, "F1", true },
        { 21, Command::softKey, 1, "F2", true },
        { 22, Command::softKey, 2, "F3", true },
        { 23, Command::softKey, 3, "F4", true },
        { 24, Command::softKey, 4, "F5", true },
        { 25, Command::softKey, 5, "F6", true },
        { 26, Command::softKey, 6, "F7", true },
        { 27, Command::softKey, 7, "F8", true },

        // --- colonna a destra dei pad: il valore della quantizzazione
        { 43, Command::quantiseValue, 0, "1/32T", true },
        { 42, Command::quantiseValue, 1, "1/32",  true },
        { 41, Command::quantiseValue, 2, "1/16T", true },
        { 40, Command::quantiseValue, 3, "1/16",  true },
        { 39, Command::quantiseValue, 4, "1/8T",  true },
        { 38, Command::quantiseValue, 5, "1/8",   true },
        { 37, Command::quantiseValue, 6, "1/4T",  true },
        { 36, Command::quantiseValue, 7, "1/4",   true },

        // --- trasporto, in basso a sinistra
        { 85, Command::play,           0, "PLAY",         true },
        { 86, Command::recordSequence, 0, "REC SEQ",      true },
        { 29, Command::stop,           0, "STOP",         true },
        { 89, Command::overdub,        0, "SOVRAINCIDI",  true },
        { 90, Command::fixedLength,    0, "BATTUTE",      false },
        { 87, Command::newSequence,    0, "NUOVA SEQ",    false },
        { 88, Command::duplicate,      0, "COPIA",        false },
        { 116, Command::timingCorrect, 0, "CORREGGI",     false },
        { 117, Command::doubleLength,  0, "RADDOPPIA",    false },
        { 35, Command::resample,       0, "RICAMPIONA",   false },

        // --- a sinistra del display
        { 118, Command::erase, 0, "CANCELLA", false },
        { 119, Command::undo,  0, "ANNULLA",  false },

        // --- sopra i pad a sinistra
        { 60, Command::padMute, 0, "MUTE", true },
        { 61, Command::padSolo, 0, "SOLO", true },

        // --- sopra il display, ai lati
        { 3,  Command::tapTempo,       0, "TAP",      false },
        { 9,  Command::metronome,      0, "METRO",    false },
        { 30, Command::setup,          0, "SETUP",    false },
        { 59, Command::handBackToLive, 0, "A LIVE",   false },

        // --- a destra del display
        { 52, Command::addEffect,    0, "AGG EFFETTO", false },
        { 53, Command::addTrack,     0, "AGG TRACCIA", false },
        { 110, Command::padParameters, 0, "PARAMETRI", false },
        { 112, Command::drumMix,      0, "MIXER",      false },
        { 111, Command::browse,       0, "SFOGLIA",    false },
        { 113, Command::sequenceList, 0, "SEQUENZE",   false },
        { 28, Command::master,        0, "GENERALE",   false },

        // --- fianco destro
        { 56, Command::noteRepeat, 0, "NOTE REPEAT", false },
        { 57, Command::fullLevel,      0, "FULL LEVEL",  false },
        { 58, Command::sixteenLevels,  0, "16 LIVELLI",  false },
        { 31, Command::layout,         0, "DISPOSIZIONE", false },
        { 50, Command::playMode,       0, "SUONA",       false },
        { 51, Command::sequenceMode,   0, "SEQUENZE",    false },
        { 55, Command::tuneUp,         0, "ACCORDA SU",  false },
        { 54, Command::tuneDown,       0, "ACCORDA GIU", false },
        { 62, Command::pageLeft,       0, "BATTUTA PREC", false },
        { 63, Command::pageRight,      0, "BATTUTA SUCC", false },
        { 49, Command::shift,          0, "SHIFT",       false },
        { 48, Command::select,         0, "SCEGLI",      false },

        // --- frecce
        { 46, Command::up,    0, "SU",      false },
        { 47, Command::down,  0, "GIU",     false },
        { 44, Command::left,  0, "SINISTRA", false },
        { 45, Command::right, 0, "DESTRA",  false },
    };
}

std::vector<Control> makeEncoders()
{
    return {
        { 14, Command::tempo,      0, "TEMPO",   false },
        { 15, Command::swing,      0, "SWING",   false },
        { 71, Command::parameter,  0, "P1",      false },
        { 72, Command::parameter,  1, "P2",      false },
        { 73, Command::parameter,  2, "P3",      false },
        { 74, Command::parameter,  3, "P4",      false },
        { 75, Command::parameter,  4, "P5",      false },
        { 76, Command::parameter,  5, "P6",      false },
        { 77, Command::parameter,  6, "P7",      false },
        { 78, Command::parameter,  7, "P8",      false },
        { 79, Command::mainVolume, 0, "VOLUME",  false },
    };
}
}

const std::vector<Control>& buttons()
{
    static const std::vector<Control> all = makeButtons();
    return all;
}

const std::vector<Control>& encoders()
{
    static const std::vector<Control> all = makeEncoders();
    return all;
}

Control forButton (int cc)
{
    for (const auto& c : buttons())
        if (c.number == cc)
            return c;
    return {};
}

Control forEncoder (int cc)
{
    for (const auto& c : encoders())
        if (c.number == cc)
            return c;
    return {};
}

juce::String quantiseName (int indexFromTop)
{
    static const char* names[] = { "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4" };
    if (! juce::isPositiveAndBelow (indexFromTop, 8))
        return {};
    return names[indexFromTop];
}

} // namespace control
