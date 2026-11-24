#include "control/Push2Panel.h"
#include "engine/SoundKind.h"
#include "machine/Library.h"
#include "machine/Machine.h"

#include <juce_core/juce_core.h>

// I colori dei pad per famiglia di suono. Si controlla che la batteria resti
// grigia, le voci celesti, e che ogni famiglia abbia il suo posto nella
// tavolozza del Push invece di accendersi tutte uguali.
class SoundKindTests final : public juce::UnitTest
{
public:
    SoundKindTests() : juce::UnitTest ("Colori dei suoni", "Macchina") {}

    void runTest() override
    {
        beginTest ("la batteria va dal blu cupo all'azzurro chiaro");
        {
            const auto kick = sound::colourSelected (sound::Kind::cassa);
            const auto snare = sound::colourSelected (sound::Kind::rullante);
            const auto cymbals = sound::colourSelected (sound::Kind::piatti);
            const auto chords = sound::colourSelected (sound::Kind::accordi);

            // tutti e tre sono blu: il blu domina sul rosso
            for (auto c : { kick, snare, cymbals })
                expect (c.getBlue() > c.getRed() + 60, "non e' blu: " + c.toString());

            // e si schiariscono salendo di tono
            expect (kick.getPerceivedBrightness() < snare.getPerceivedBrightness(),
                    "la cassa non e' piu' cupa del rullante");
            expect (snare.getPerceivedBrightness() < cymbals.getPerceivedBrightness(),
                    "i piatti non sono piu' chiari del rullante");

            // i piatti sono azzurri, non blu pieni: anche il verde e' alto
            expect (cymbals.getGreen() > cymbals.getRed() + 60, "i piatti non sono azzurri");

            // verde scuro: il verde domina e la luminosita' e' bassa
            expect (chords.getGreen() > chords.getRed() + 40, "gli accordi non sono verdi");
            expect (chords.getBrightness() < 0.7f, "il verde degli accordi e' troppo chiaro");

            // niente arancioni o viola in giro
            for (int i = 0; i < sound::numKinds; ++i)
            {
                const auto c = sound::colourSelected (sound::kindFromIndex (i));
                const bool arancione = c.getRed() > c.getBlue() + 40 && c.getRed() > c.getGreen() + 40;
                expect (! arancione, "colore fuori stile: " + c.toString());
            }
        }

        beginTest ("ogni stato del pad ha la sua intensita'");
        {
            const auto kind = sound::Kind::giro;
            // si guarda il chiaro come lo vede l'occhio: un blu pieno ha la
            // luminosita' al massimo ma resta scuro da vedere
            expect (sound::colour (kind).getPerceivedBrightness()
                        < sound::colourSelected (kind).getPerceivedBrightness(),
                    "il pad fermo non e' piu' spento di quello scelto");
            expect (sound::colourPlaying (kind).getPerceivedBrightness()
                        > sound::colourSelected (kind).getPerceivedBrightness(),
                    "il pad che suona non e' piu' acceso");
        }

        beginTest ("la famiglia si indovina dal nome del campione");
        {
            expect (sound::guessFromName ("CASSA") == sound::Kind::cassa);
            expect (sound::guessFromName ("kick 01.wav") == sound::Kind::cassa);
            expect (sound::guessFromName ("ST2AMBSN2") == sound::Kind::rullante,
                    "il rullante di un kit vero non e' stato riconosciuto");
            expect (sound::guessFromName ("AMB_CLHH3") == sound::Kind::piatti);
            expect (sound::guessFromName ("16AMB_CRS1") == sound::Kind::piatti);
            expect (sound::guessFromName ("R_AMBTOMF1") == sound::Kind::tom);
            expect (sound::guessFromName ("VOX LEAD") == sound::Kind::voce);
            expect (sound::guessFromName ("BASS 808") == sound::Kind::basso);
            expect (sound::guessFromName ("PIANO CHORD") == sound::Kind::accordi);
            expect (sound::guessFromName ("BREAK 4") == sound::Kind::giro);
            expect (sound::guessFromName ("CONGA") == sound::Kind::percussioni);
            expect (sound::guessFromName ("qualcosa") == sound::Kind::generico);
        }

        beginTest ("nella tavolozza del Push ogni famiglia ha il suo posto");
        {
            const int entries = Push2Panel::numberOfPaletteEntries();
            expect (entries >= 2 + sound::numKinds * 3, "troppe poche voci: " + juce::String (entries));
            expect (entries <= 120, "la tavolozza del Push non ci sta");

            std::vector<int> seen;
            for (int i = 0; i < sound::numKinds; ++i)
            {
                const auto kind = sound::kindFromIndex (i);
                const int loaded = Push2Panel::paletteIndexFor (machine::PadLight::loaded, kind);
                const int selected = Push2Panel::paletteIndexFor (machine::PadLight::selected, kind);
                const int playing = Push2Panel::paletteIndexFor (machine::PadLight::playing, kind);

                for (int index : { loaded, selected, playing })
                {
                    expect (index > 0 && index <= entries, "indice fuori posto: " + juce::String (index));
                    for (int other : seen)
                        expect (other != index, "due famiglie con lo stesso colore");
                    seen.push_back (index);
                }
            }

            // vuoto e mutato non dipendono dalla famiglia
            expectEquals (Push2Panel::paletteIndexFor (machine::PadLight::empty, sound::Kind::voce),
                          Push2Panel::paletteIndexFor (machine::PadLight::empty, sound::Kind::basso));
            expectEquals (Push2Panel::paletteIndexFor (machine::PadLight::muted, sound::Kind::voce),
                          Push2Panel::paletteIndexFor (machine::PadLight::muted, sound::Kind::basso));
        }

        beginTest ("il kit di partenza e' tutto batteria e percussioni");
        {
            machine::Machine m;
            m.prepare (44100.0);

            for (int pad = 1; pad <= 8; ++pad)
            {
                const machine::PadRef p { 0, pad };
                const auto kind = m.kindFor (p);
                expect (kind == sound::Kind::cassa || kind == sound::Kind::rullante
                            || kind == sound::Kind::piatti || kind == sound::Kind::tom
                            || kind == sound::Kind::percussioni,
                        "pad " + juce::String (pad) + ": " + sound::name (kind));
            }
        }

        beginTest ("il rotativo cambia la famiglia a mano");
        {
            machine::Machine m;
            m.prepare (44100.0);

            for (const auto& c : control::buttons())
                if (c.command == control::Command::pageSound)
                {
                    m.command (c, true);
                    m.command (c, false);
                }

            const auto before = m.kindFor (m.selectedPad());
            m.encoder (control::forEncoder (71 + 7), 1);
            expect (! (m.kindFor (m.selectedPad()) == before), "la famiglia non e' cambiata");
        }

        beginTest ("la scena si ricorda la famiglia dei suoni");
        {
            auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("rebirth-colori-" + juce::String (juce::Random::getSystemRandom().nextInt (100000)));
            temp.createDirectory();

            machine::Machine m;
            m.prepare (44100.0);

            juce::AudioBuffer<float> b (1, 1000);
            for (int i = 0; i < 1000; ++i)
                b.setSample (0, i, 0.5f);

            // un nome che non fa indovinare niente, cosi' si prova davvero che
            // la famiglia viene dal file della scena
            expect (m.engine().loadBuffer (40, "ZZZ", b, 44100.0, sound::Kind::voce));
            expect (m.kindFor (machine::drumAt (40)) == sound::Kind::voce);

            juce::String error;
            const auto folder = temp.getChildFile ("colori");
            expect (library::saveScene (folder, m, error), error);

            machine::Machine other;
            other.prepare (44100.0);
            expect (library::loadScene (folder, other, error), error);
            expect (other.kindFor (machine::drumAt (40)) == sound::Kind::voce,
                    "la famiglia non e' tornata: " + sound::name (other.kindFor (machine::drumAt (40))));

            temp.deleteRecursively();
        }
    }
};

static SoundKindTests soundKindTests;
