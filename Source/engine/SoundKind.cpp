#include "SoundKind.h"

namespace sound
{

namespace
{
struct Row
{
    Kind kind;
    const char* name;
    std::uint32_t colour;
};

const Row table[] =
{
    { Kind::generico,    "SUONO",       0xff4a4d52 },   // grigio scuro
    { Kind::batteria,    "BATTERIA",    0xff9b9da2 },   // grigio
    { Kind::percussioni, "PERCUSSIONI", 0xff6f757c },   // grigio bluastro
    { Kind::voce,        "VOCE",        0xff5fc8ff },   // celeste
    { Kind::giro,        "GIRI",        0xff3d8bff },   // blu
    { Kind::accordi,     "ACCORDI",     0xff1f8a4c },   // verde scuro
    { Kind::basso,       "BASSO",       0xff1b3fa0 },   // blu notte
    { Kind::registrato,  "REGISTRATO",  0xffd6d8dc }    // grigio chiaro
};

const Row& rowFor (Kind k)
{
    for (const auto& row : table)
        if (row.kind == k)
            return row;

    return table[0];
}
}

Kind kindFromIndex (int index)
{
    if (juce::isPositiveAndBelow (index, numKinds))
        return table[index].kind;

    return Kind::generico;
}

int indexOf (Kind k)
{
    for (int i = 0; i < numKinds; ++i)
        if (table[i].kind == k)
            return i;

    return 0;
}

juce::String name (Kind k)
{
    return rowFor (k).name;
}

juce::Colour colour (Kind k)
{
    // fermo: il colore della famiglia, tenuto giu' cosi' il pad scelto e quello
    // che suona si vedono sopra gli altri
    return juce::Colour (rowFor (k).colour).withMultipliedBrightness (0.6f);
}

juce::Colour colourSelected (Kind k)
{
    return juce::Colour (rowFor (k).colour);
}

juce::Colour colourPlaying (Kind k)
{
    // che suona: lo stesso colore tirato verso il bianco, cosi' il colpo si
    // vede anche in mezzo a una griglia piena. Non basta alzare la
    // luminosita': un blu pieno e' gia' al massimo e non schiarirebbe.
    return juce::Colour (rowFor (k).colour).interpolatedWith (juce::Colours::white, 0.45f);
}

Kind guessFromName (const juce::String& text)
{
    const auto t = text.toUpperCase();

    const auto has = [&t] (const char* word) { return t.contains (word); };

    if (has ("CASSA") || has ("KICK") || has ("RULLANTE") || has ("SNARE")
            || has ("CHARLES") || has ("HAT") || has ("HIHAT") || has ("BATT")
            || has ("DRUM") || has ("TOM") || has ("RIM") || has ("CLAP")
            || has ("BATTIMANI") || has ("CRASH") || has ("RIDE"))
        return Kind::batteria;

    if (has ("PERC") || has ("CONGA") || has ("BONGO") || has ("SHAKER")
            || has ("TAMB") || has ("CAMPANA") || has ("COWBELL") || has ("BLOCK"))
        return Kind::percussioni;

    if (has ("VOCE") || has ("VOX") || has ("VOC") || has ("CORO")
            || has ("CANTO") || has ("PAROLA") || has ("ACAPELLA") || has ("VOICE"))
        return Kind::voce;

    if (has ("BASSO") || has ("BASS") || has ("SUB") || has ("808"))
        return Kind::basso;

    if (has ("ACCORDO") || has ("ACCORDI") || has ("CHORD") || has ("PAD ")
            || has ("TASTIER") || has ("PIANO") || has ("ORGANO") || has ("ARCHI")
            || has ("STRING") || has ("KEY"))
        return Kind::accordi;

    if (has ("REG ") || has ("REGISTR") || has ("MIC") || has ("TAKE"))
        return Kind::registrato;

    if (has ("GIRO") || has ("GIRI") || has ("LOOP") || has ("BREAK")
            || has ("FETTA") || has ("BRANO") || has ("GROOVE"))
        return Kind::giro;

    return Kind::generico;
}

} // namespace sound
