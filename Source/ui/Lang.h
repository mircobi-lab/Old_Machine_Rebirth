#pragma once

#include <juce_core/juce_core.h>

// Le tre lingue della macchina.
//
// Le parole scritte nel codice sono quelle italiane: valgono da chiave e si
// vedono tali e quali quando la lingua e' l'italiano. Per l'inglese e il
// francese c'e' una tabella; una parola che non sta in tabella resta in
// italiano, cosi' niente sparisce mai dal display.
//
// Sul pannello e sul display si scrive in maiuscolo, come la serigrafia di una
// macchina vera, e in francese il maiuscolo va senza accenti: niente caratteri
// strani da far digerire ai tre compilatori.
namespace lang
{

enum class Language { italiano, english, francais };

void set (Language);
Language current();

juce::String name (Language);              // ITA, UK, FR
juce::String nextName();                   // la prossima nel giro

// La parola nella lingua scelta.
juce::String t (const juce::String& italian);

// Una frase composta, tipo "PASSO 1/16" o "ANNULLATO FETTE": traduce il pezzo
// piu' lungo che trova in tabella e tira avanti con quello che resta, cosi' i
// numeri e i nomi dei campioni passano intatti.
juce::String phrase (const juce::String&);

// Comodo per i messaggi: "FETTE 16" da "FETTE" piu' il numero.
juce::String t (const juce::String& italian, const juce::String& tail);

} // namespace lang
