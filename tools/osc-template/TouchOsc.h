#pragma once

#include <juce_core/juce_core.h>

// Scrive il banco di comandi per TouchOSC Mk1: un file .touchosc, che e' uno
// zip con dentro un solo index.xml. Nel formato Mk1 i nomi, i testi e gli
// indirizzi OSC stanno scritti in base64.
namespace touchosc
{
bool write (const juce::File& destination);
}
