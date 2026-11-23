#include "Machine.h"
#include "ui/Lang.h"

namespace machine
{

// La colonna a destra dei pad, dall'alto: 1/32T, 1/32, 1/16T, 1/16, 1/8T, 1/8,
// 1/4T, 1/4. Qui diventa quanto dura un passo, in battute da un quarto.
double quantiseBeats (int indexFromTop)
{
    static const double beats[] = { 1.0 / 12.0, 1.0 / 8.0, 1.0 / 6.0, 1.0 / 4.0,
                                    1.0 / 3.0,  1.0 / 2.0, 2.0 / 3.0, 1.0 };
    if (! juce::isPositiveAndBelow (indexFromTop, 8))
        return 0.25;
    return beats[indexFromTop];
}

Machine::Machine()
{
    sampler.loadStarterKit();
    mutes.fill (false);
    held.fill (false);
}

void Machine::prepare (double sampleRate)
{
    rec.prepare (sampleRate, 2);
    rate = sampleRate > 0.0 ? sampleRate : 44100.0;
    sampler.prepare (rate);
    stepCounter = 0.0;
    clickRemaining = 0;
}

void Machine::changed()
{
    if (onChanged != nullptr)
        onChanged();
}

void Machine::setMessage (juce::String text)
{
    message = std::move (text);
}

//==============================================================================
void Machine::process (juce::AudioBuffer<float>& buffer)
{
    const double beatsPerStep = quantiseBeats (quantise);
    const double secondsPerBeat = 60.0 / juce::jmax (20.0, tempoAtomic.load());
    samplesPerStep = juce::jmax (1.0, beatsPerStep * secondsPerBeat * rate);

    const int numSamples = buffer.getNumSamples();

    // Note repeat e metronomo battono il tempo dentro il filo dell'audio, cosi'
    // non dipendono da quanto e' puntuale il filo dei messaggi.
    if (repeatRunning.load() || metronomeOn.load())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            stepCounter += 1.0;
            if (stepCounter < samplesPerStep)
                continue;

            stepCounter -= samplesPerStep;

            if (repeatRunning.load())
            {
                const int d = repeatDrum.load();
                if (d >= 0)
                    sampler.trigger (d, (float) repeatVelocity.load() / 127.0f);
            }

            if (metronomeOn.load())
                clickRemaining = (int) (0.02 * rate);
        }
    }
    else
        stepCounter = 0.0;

    sampler.process (buffer);

    // il clic del metronomo, generato qui e non da un campione
    if (clickRemaining > 0 && buffer.getNumChannels() > 0)
    {
        const int n = juce::jmin (clickRemaining, numSamples);
        for (int i = 0; i < n; ++i)
        {
            clickPhase += juce::MathConstants<double>::twoPi * 1000.0 / rate;
            const float env = (float) clickRemaining / (float) (0.02 * rate);
            const float s = (float) std::sin (clickPhase) * 0.18f * env * env;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.addSample (ch, i, s);
            --clickRemaining;
        }
    }

    buffer.applyGain (masterGain);
}

//==============================================================================
void Machine::select (PadRef p)
{
    selected = p;
    changed();
}

void Machine::hit (PadRef p, int velocity)
{
    const int index = drumIndex (p);

    if (muteMode)
    {
        mutes[(size_t) index] = ! mutes[(size_t) index];
        setMessage (juce::String (mutes[(size_t) index] ? "MUTO " : "SUONA ")
                        + bankName (p.bank) + juce::String (p.pad));
        select (p);
        return;
    }

    if (eraseMode)
    {
        pushUndo (index, "CANCELLA");
        sampler.drum (index) = Drum();
        setMessage ("CANCELLATO " + bankName (p.bank) + juce::String (p.pad));
        select (p);
        return;
    }

    if (copySource >= 0)
    {
        pushUndo (index, "COPIA");
        sampler.drum (index) = sampler.drum (copySource);
        setMessage ("COPIATO SU " + bankName (p.bank) + juce::String (p.pad));
        copySource = -1;
        select (p);
        return;
    }

    if (mutes[(size_t) index])
    {
        select (p);
        return;
    }

    int playIndex = index;
    float gain = fullLevel ? 1.0f : juce::jlimit (0.0f, 1.0f, (float) velocity / 127.0f);

    if (sixteenLevels)
    {
        // 16 LEVELS: un suono solo su tutti i pad del quadrante, a sedici
        // gradini di volume oppure di accordatura, come sull'MPC60.
        playIndex = drumIndex (selected);
        const int step = (p.pad - 1);     // 0..15

        if (levelsType == LevelsType::volume)
            gain = (float) (step + 1) / 16.0f;
        else
        {
            auto& d = sampler.drum (playIndex);
            const int saved = d.tuneSemitones;
            d.tuneSemitones = juce::jlimit (-24, 24, step - 8);
            sampler.trigger (playIndex, gain);
            d.tuneSemitones = saved;
            changed();
            return;
        }
    }
    else
        select (p);

    sampler.trigger (playIndex, gain);
    changed();
}

void Machine::padDown (PadRef p, int velocity)
{
    const int index = drumIndex (p);
    held[(size_t) index] = true;

    if (noteRepeat && ! muteMode && ! eraseMode && copySource < 0)
    {
        // col note repeat il pad ribatte da solo al valore scelto nella colonna
        repeatDrum.store (sixteenLevels ? drumIndex (selected) : index);
        repeatVelocity.store (fullLevel ? 127 : velocity);
        repeatRunning.store (true);
        select (p);
        return;
    }

    hit (p, velocity);
}

void Machine::padUp (PadRef p)
{
    const int index = drumIndex (p);
    held[(size_t) index] = false;

    if (repeatRunning.load() && repeatDrum.load() == index)
    {
        bool anyHeld = false;
        for (bool h : held)
            anyHeld = anyHeld || h;
        if (! anyHeld)
            repeatRunning.store (false);
    }

    changed();
}

//==============================================================================
void Machine::pushUndo (int index, const juce::String& what)
{
    undoStack.push_back ({ index, sampler.drum (index), what });
    if (undoStack.size() > 32)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void Machine::undo()
{
    if (undoStack.empty())
    {
        setMessage ("NIENTE DA ANNULLARE");
        return;
    }

    auto last = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back ({ last.drumIndex, sampler.drum (last.drumIndex), last.what });
    sampler.drum (last.drumIndex) = last.drum;
    setMessage ("ANNULLATO " + last.what);
    changed();
}

void Machine::redo()
{
    if (redoStack.empty())
    {
        setMessage ("NIENTE DA RIFARE");
        return;
    }

    auto last = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back ({ last.drumIndex, sampler.drum (last.drumIndex), last.what });
    sampler.drum (last.drumIndex) = last.drum;
    setMessage ("RIFATTO " + last.what);
    changed();
}

void Machine::tapTempo()
{
    const auto now = juce::Time::currentTimeMillis();
    const double gap = (double) (now - lastTap);
    lastTap = now;

    if (gap > 2000.0 || gap < 150.0)
    {
        tapIntervals.clear();
        return;
    }

    tapIntervals.push_back (gap);
    if (tapIntervals.size() > 4)
        tapIntervals.erase (tapIntervals.begin());

    double sum = 0.0;
    for (double g : tapIntervals)
        sum += g;

    bpm = juce::jlimit (30.0, 300.0, 60000.0 / (sum / (double) tapIntervals.size()));
    tempoAtomic.store (bpm);
    setMessage ("TEMPO " + juce::String (juce::roundToInt (bpm)));
    changed();
}

void Machine::setBankGroup (int g)
{
    group = juce::jlimit (0, 1, g);
    changed();
}

//==============================================================================
void Machine::command (const control::Control& c, bool down)
{
    using C = control::Command;

    if (c.command == C::shift)
    {
        shift = down;
        return;
    }

    if (! down)
    {
        // il note repeat dura quanto si tiene premuto il tasto
        if (c.command == C::noteRepeat)
        {
            noteRepeat = false;
            repeatRunning.store (false);
            changed();
        }
        return;
    }

    setMessage (c.label);

    switch (c.command)
    {
        case C::pageSample: case C::pageChop: case C::pageSound: case C::pageSequence:
        case C::pageStep: case C::pageSong: case C::pageMix: case C::pageDisk:
            page = c.command;
            pageName = c.label;
            break;

        case C::softKey:
            if (page == C::pageSample)
            {
                if (c.index == 0)
                {
                    rec.arm();
                    setMessage ("ARMATO SOGLIA " + juce::String (rec.threshold()));
                }
                else if (c.index == 1)
                {
                    rec.startNow();
                    setMessage ("REGISTRO");
                }
                else if (c.index == 2)
                {
                    rec.stop();
                    setMessage (rec.hasTake() ? "REGISTRATO" : "FERMO");
                    checkRecorder();
                }
                else if (c.index == 3)
                {
                    rec.setMonitoring (! rec.monitoring());
                    setMessage ("ASCOLTA " + juce::String (rec.monitoring() ? "ACCESO" : "SPENTO"));
                }
                else
                    setMessage ("");
            }
            else if (page == C::pageDisk)
            {
                if (c.index == 0 && onSaveScene)        onSaveScene();
                else if (c.index == 1 && onOpenScene)   onOpenScene();
                else if (c.index == 2 && onToLibrary)   onToLibrary();
                else if (c.index == 3 && onBrowseLibrary) onBrowseLibrary();
                else setMessage ("");
            }
            else if (page == C::pageChop)
            {
                if (c.index == 0)
                {
                    setMessage ("SPACCO IL BRANO");
                    if (onChopRequested)
                        onChopRequested();
                }
                else if (c.index == 1 || c.index == 2)
                {
                    const auto preset = c.index == 1 ? cleaner::Preset::ai : cleaner::Preset::mp3;
                    if (! sampler.drum (drumIndex (selected)).loaded())
                        setMessage ("PAD VUOTO");
                    else
                    {
                        setMessage ("PULISCO " + cleaner::presetName (preset));
                        if (onCleanRequested)
                            onCleanRequested (preset);
                    }
                }
                else if (c.index == 3)
                    undo();
                else if (c.index == 4)
                {
                    // il campione lungo che sta sul pad si taglia subito; se il
                    // pad e' vuoto si chiede un file
                    if (sampler.drum (drumIndex (selected)).loaded())
                        fillFromSelected();
                    else if (onFillRequested)
                    {
                        setMessage ("RIEMPI");
                        onFillRequested();
                    }
                }
                else if (c.index == 5 || c.index == 6)
                {
                    slices = slicer::nextCount (slices, c.index == 5 ? 1 : -1);
                    setMessage ("FETTE " + juce::String (slices));
                }
                else
                    setMessage ("");
            }
            break;

        case C::quantiseValue:
            quantise = c.index;
            setMessage ("PASSO " + control::quantiseName (c.index));
            break;

        case C::layout:
            setBankGroup (group == 0 ? 1 : 0);
            setMessage (group == 0 ? "BANCHI A-D" : "BANCHI E-H");
            break;

        case C::fullLevel:
            fullLevel = ! fullLevel;
            setMessage (fullLevel ? "FULL LEVEL ACCESO" : "FULL LEVEL SPENTO");
            break;

        case C::sixteenLevels:
            if (shift)
            {
                levelsType = levelsType == LevelsType::volume ? LevelsType::tune : LevelsType::volume;
                setMessage (levelsType == LevelsType::volume ? "16 LIVELLI: VOLUME"
                                                             : "16 LIVELLI ACCORDA");
            }
            else
            {
                sixteenLevels = ! sixteenLevels;
                setMessage (sixteenLevels ? "16 LIVELLI ACCESO" : "16 LIVELLI SPENTO");
            }
            break;

        case C::padMute:
            if (shift)
            {
                mutes.fill (false);
                setMessage ("TUTTI SUONANO");
            }
            else
            {
                muteMode = ! muteMode;
                setMessage (muteMode ? "MUTE SCEGLI I PAD" : "MUTE SPENTO");
            }
            break;

        case C::erase:
            if (shift)
            {
                copySource = drumIndex (selected);
                setMessage ("COPIA BATTI IL PAD");
            }
            else
            {
                eraseMode = ! eraseMode;
                setMessage (eraseMode ? "CANCELLA SCEGLI I PAD" : "CANCELLA SPENTO");
            }
            break;

        case C::noteRepeat:
            noteRepeat = true;
            setMessage ("NOTE REPEAT " + control::quantiseName (quantise));
            break;

        case C::undo:
            shift ? redo() : undo();
            break;

        case C::tapTempo:
            tapTempo();
            break;

        case C::metronome:
            metronome = ! metronome;
            metronomeOn.store (metronome);
            setMessage (metronome ? "METRONOMO ACCESO" : "METRONOMO SPENTO");
            break;

        case C::tuneUp:
        case C::tuneDown:
        {
            auto& d = sampler.drum (drumIndex (selected));
            // col tasto SHIFT si va di ottava, come mezza e doppia velocita'
            const int amount = shift ? 12 : 1;
            d.tuneSemitones = juce::jlimit (-24, 24,
                d.tuneSemitones + (c.command == C::tuneUp ? amount : -amount));
            setMessage ("ACCORDA " + juce::String (d.tuneSemitones > 0 ? "+" : "")
                            + juce::String (d.tuneSemitones));
            break;
        }

        case C::stop:
            sampler.stopAll();
            repeatRunning.store (false);
            setMessage ("FERMO");
            break;

        case C::select:
            // SELECT: il pad si sceglie senza farlo suonare
            setMessage ("SCEGLI SENZA SUONARE");
            break;

        default:
            break;
    }

    changed();
}

void Machine::changeParameter (int which, int steps)
{
    // nella pagina CAMPIONA i primi tre rotativi sono quelli del registratore,
    // perche' li' si guardano la soglia e la durata, non il campione
    if (page == control::Command::pageSample && which <= 2)
    {
        if (which == 0)
        {
            rec.setThreshold (rec.threshold() + steps);
            setMessage ("SOGLIA " + juce::String (rec.threshold()));
        }
        else if (which == 1)
        {
            rec.setLengthSeconds (rec.lengthSeconds() + steps * 0.5);
            setMessage ("DURATA " + juce::String (rec.lengthSeconds(), 1) + "S");
        }
        else
        {
            rec.setPreRollSeconds (rec.preRollSeconds() + steps * 0.05);
            setMessage ("ANTICIPO "
                            + juce::String (juce::roundToInt (rec.preRollSeconds() * 1000.0)) + "MS");
        }

        return;
    }

    auto& d = sampler.drum (drumIndex (selected));
    const int length = d.sample.getNumSamples();

    switch (which)
    {
        case 0:
            d.level = juce::jlimit (0.0f, 1.0f, d.level + (float) steps * 0.02f);
            setMessage ("LIVELLO " + juce::String (juce::roundToInt (d.level * 100.0f)));
            break;

        case 1:
            d.tuneSemitones = juce::jlimit (-24, 24, d.tuneSemitones + steps);
            setMessage ("ACCORDA " + juce::String (d.tuneSemitones > 0 ? "+" : "")
                            + juce::String (d.tuneSemitones));
            break;

        case 2:
            d.pan = juce::jlimit (-1.0f, 1.0f, d.pan + (float) steps * 0.05f);
            setMessage ("PANORAMA " + juce::String (juce::roundToInt (d.pan * 100.0f)));
            break;

        case 3:
            d.start = juce::jlimit (0, juce::jmax (0, d.lastSample() - 2),
                                    d.start + steps * juce::jmax (1, length / 200));
            setMessage ("INIZIO " + juce::String (d.start));
            break;

        case 4:
            d.end = juce::jlimit (juce::jmin (d.start + 2, length), length,
                                  (d.end > 0 ? d.end : length) + steps * juce::jmax (1, length / 200));
            setMessage ("FINE " + juce::String (d.lastSample()));
            break;

        case 5:
            d.muteGroup = juce::jlimit (0, 8, d.muteGroup + steps);
            setMessage (d.muteGroup == 0 ? "NESSUN GRUPPO"
                                         : "GRUPPO " + juce::String (d.muteGroup));
            break;

        case 6:
            if (steps != 0)
            {
                d.reverse = steps > 0;
                setMessage (d.reverse ? "ROVESCIO" : "DRITTO");
            }
            break;

        case 7:
            bpm = juce::jlimit (30.0, 300.0, bpm + steps);
            tempoAtomic.store (bpm);
            setMessage ("TEMPO " + juce::String (juce::roundToInt (bpm)));
            break;

        default:
            break;
    }
}

void Machine::encoder (const control::Control& c, int steps)
{
    using C = control::Command;

    if (steps == 0)
        return;

    switch (c.command)
    {
        case C::parameter:
            changeParameter (c.index, steps);
            break;

        case C::mainVolume:
            masterGain = juce::jlimit (0.0f, 1.0f, masterGain + (float) steps * 0.02f);
            setMessage ("VOLUME " + juce::String (juce::roundToInt (masterGain * 100.0f)));
            break;

        case C::tempo:
            bpm = juce::jlimit (30.0, 300.0, bpm + steps);
            tempoAtomic.store (bpm);
            setMessage ("TEMPO " + juce::String (juce::roundToInt (bpm)));
            break;

        case C::swing:
            setMessage ("SWING");     // arrivera' col sequencer
            break;

        default:
            break;
    }

    changed();
}

//==============================================================================
PadLight Machine::lightFor (PadRef p) const
{
    const int index = drumIndex (p);

    if (mutes[(size_t) index])
        return PadLight::muted;
    if (index == drumIndex (selected))
        return PadLight::selected;
    if (sixteenLevels)
        return PadLight::loaded;     // tutto il quadrante suona il suono scelto
    return sampler.drum (index).loaded() ? PadLight::loaded : PadLight::empty;
}

bool Machine::buttonLit (const control::Control& c) const
{
    using C = control::Command;

    switch (c.command)
    {
        case C::pageSample: case C::pageChop: case C::pageSound: case C::pageSequence:
        case C::pageStep: case C::pageSong: case C::pageMix: case C::pageDisk:
            return c.command == page;
        case C::quantiseValue:  return c.index == quantise;
        case C::fullLevel:      return fullLevel;
        case C::sixteenLevels:  return sixteenLevels;
        case C::padMute:        return muteMode;
        case C::erase:          return eraseMode || copySource >= 0;
        case C::noteRepeat:     return noteRepeat;
        case C::metronome:      return metronome;
        case C::layout:         return group == 1;
        case C::softKey:        return softKeyLabels()[c.index].isNotEmpty();
        default:                return false;
    }
}

bool Machine::checkRecorder()
{
    juce::AudioBuffer<float> recorded;
    if (! rec.takeRecording (recorded))
        return false;

    const int index = drumIndex (selected);
    pushUndo (index, lang::t ("REGISTRA"));

    const auto name = "REG " + juce::String (++recordings);
    if (! sampler.loadBuffer (index, name, recorded, rate))
        return false;

    setMessage ("REGISTRATO " + juce::String (recorded.getNumSamples() / juce::jmax (1.0, rate), 1) + "S");
    changed();
    return true;
}

juce::StringArray Machine::softKeyLabels() const
{
    juce::StringArray labels;
    for (int i = 0; i < 8; ++i)
        labels.add ("");

    if (page == control::Command::pageSample)
    {
        labels.set (0, lang::t ("ARMA"));
        labels.set (1, lang::t ("REGISTRA"));
        labels.set (2, lang::t ("STOP"));
        labels.set (3, lang::t ("ASCOLTA"));
    }
    else if (page == control::Command::pageDisk)
    {
        labels.set (0, lang::t ("SALVA SCENA"));
        labels.set (1, lang::t ("APRI SCENA"));
        labels.set (2, lang::t ("IN LIBRERIA"));
        labels.set (3, lang::t ("LIBRERIA"));
    }
    else if (page == control::Command::pageChop)
    {
        labels.set (0, lang::t ("SPACCA"));
        labels.set (1, lang::t ("PULISCI AI"));
        labels.set (2, lang::t ("PULISCI MP3"));
        labels.set (3, lang::t ("ANNULLA"));
        labels.set (4, lang::t ("RIEMPI"));
        labels.set (5, lang::t ("FETTE +"));
        labels.set (6, lang::t ("FETTE -"));
    }

    return labels;
}

int Machine::placeSlices (const std::vector<chopper::Slice>& slices)
{
    if (slices.empty())
        return 0;

    int placed = 0;
    int next = 0;                       // primo pad libero, in indici di suono
    juce::String lastBank;

    for (const auto& slice : slices)
    {
        if (slice.audio.getNumSamples() <= 0)
            continue;

        // ogni banco dell'analisi comincia su una fila nuova
        if (slice.bank != lastBank && placed > 0)
            next = ((next + 7) / 8) * 8;
        lastBank = slice.bank;

        if (next >= numDrums)
            break;

        pushUndo (next, "FETTE");
        if (sampler.loadBuffer (next, slice.name, slice.audio, slice.sampleRate))
            ++placed;

        ++next;
    }

    if (placed > 0)
    {
        select ({ 0, 1 });
        setMessage (juce::String (placed) + " FETTE SUI PAD");
        changed();
    }

    return placed;
}

Machine::State Machine::snapshot() const
{
    State s;
    s.bpm = bpm;
    s.quantise = quantise;
    s.bankGroup = group;
    s.selectedPad = drumIndex (selected);
    s.masterGain = masterGain;
    s.slices = slices;
    s.mutes = mutes;
    return s;
}

void Machine::restore (const State& s)
{
    bpm = juce::jlimit (30.0, 300.0, s.bpm);
    tempoAtomic.store (bpm);
    quantise = juce::jlimit (0, 7, s.quantise);
    masterGain = juce::jlimit (0.0f, 1.0f, s.masterGain);
    slices = s.slices > 0 ? s.slices : 16;
    mutes = s.mutes;

    setBankGroup (juce::jlimit (0, 1, s.bankGroup));

    select (drumAt (juce::jlimit (0, numDrums - 1, s.selectedPad)));

    changed();
}

void Machine::clearAllPads()
{
    for (int i = 0; i < numDrums; ++i)
        sampler.drum (i) = Drum();

    undoStack.clear();
    redoStack.clear();
    changed();
}

int Machine::placeFrom (PadRef start, const std::vector<chopper::Slice>& list)
{
    if (list.empty())
        return 0;

    int at = drumIndex (start);
    int placed = 0;

    for (const auto& slice : list)
    {
        if (at >= numDrums)
            break;

        if (slice.audio.getNumSamples() <= 0)
            continue;

        pushUndo (at, lang::t ("RIEMPI"));
        if (sampler.loadBuffer (at, slice.name, slice.audio, slice.sampleRate))
            ++placed;

        ++at;
    }

    if (placed > 0)
    {
        select (start);
        setMessage (juce::String (placed) + " FETTE SUI PAD");
        changed();
    }

    return placed;
}

int Machine::fillFromSelected()
{
    const int index = drumIndex (selected);
    const auto& d = sampler.drum (index);
    if (! d.loaded())
    {
        setMessage ("PAD VUOTO");
        changed();
        return 0;
    }

    const auto source = sampler.copyOf (index);
    const auto name = d.name;
    const auto sourceRate = d.sourceRate;

    auto list = slicer::cut (source, sourceRate, slices, name);
    if (list.empty())
    {
        setMessage ("NIENTE DA SPACCARE");
        changed();
        return 0;
    }

    return placeFrom (selected, list);
}

bool Machine::applyCleanedSample (int index, const juce::AudioBuffer<float>& buffer,
                                  double sourceRate, const juce::String& what)
{
    if (buffer.getNumSamples() <= 0 || ! juce::isPositiveAndBelow (index, numDrums))
        return false;

    const auto name = sampler.drum (index).name;
    pushUndo (index, what);
    if (! sampler.loadBuffer (index, name, buffer, sourceRate))
        return false;

    setMessage ("PULITO " + what);
    changed();
    return true;
}

juce::StringArray Machine::screenLines() const
{
    const auto& d = sampler.drum (drumIndex (selected));

    juce::StringArray lines;
    lines.add ("#" + lang::phrase (pageName));
    lines.add (lang::t ("PAD") + "|" + bankName (selected.bank) + juce::String (selected.pad));
    lines.add (lang::t ("SUONO") + "|" + (d.loaded() ? d.name : lang::t ("VUOTO")));
    if (page == control::Command::pageSample)
    {
        lines.add (lang::t ("SOGLIA") + "|" + juce::String (rec.threshold()));
        lines.add (lang::t ("DURATA") + "|" + juce::String (rec.lengthSeconds(), 1) + "S");
        lines.add (lang::t ("ANTICIPO") + "|"
                       + juce::String (juce::roundToInt (rec.preRollSeconds() * 1000.0)) + "MS");
        lines.add (lang::t ("INGRESSO") + "|"
                       + juce::String (juce::roundToInt (rec.inputLevel() * 100.0f)));
    }
    else
    {
        lines.add (lang::t ("LIVELLO") + "|" + juce::String (juce::roundToInt (d.level * 100.0f)));
        lines.add (lang::t ("ACCORDA") + "|" + juce::String (d.tuneSemitones > 0 ? "+" : "")
                       + juce::String (d.tuneSemitones));
        lines.add (lang::t ("PANORAMA") + "|" + juce::String (juce::roundToInt (d.pan * 100.0f)));
        lines.add (lang::t ("PASSO") + "|" + control::quantiseName (quantise));
    }
    lines.add (lang::t ("TEMPO") + "|" + juce::String (juce::roundToInt (bpm)));
    lines.add (lang::t ("STATO") + "|" + lang::phrase (message.isEmpty() ? juce::String ("PRONTO") : message));
    return lines;
}

} // namespace machine
