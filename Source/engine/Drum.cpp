#include "Drum.h"

juce::String playModeName (PlayMode mode)
{
    switch (mode)
    {
        case PlayMode::tenuto:     return "TENUTO";
        case PlayMode::giro:       return "GIRO";
        case PlayMode::avviaFerma: return "AVVIA FERMA";
        default:                   return "UNA VOLTA";
    }
}

PlayMode playModeFromName (const juce::String& name)
{
    const auto n = name.toUpperCase();
    if (n == "TENUTO")      return PlayMode::tenuto;
    if (n == "GIRO")        return PlayMode::giro;
    if (n == "AVVIA FERMA") return PlayMode::avviaFerma;
    return PlayMode::unaVolta;
}
