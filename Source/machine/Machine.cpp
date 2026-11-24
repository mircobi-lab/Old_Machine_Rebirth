#include "Machine.h"
#include "engine/Warp.h"
#include "push2/Push2Protocol.h"
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
    seq.prepare (sampleRate);
    comp.prepare (sampleRate);
    applyQuantiseToSequencer();
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
    const bool countingIn = seq.isCountingIn();
    if (repeatRunning.load() || metronomeOn.load() || countingIn)
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

            // durante l'avvio contato il metronomo batte comunque: e' tutto
            // quello che si sente in quelle battute
            if (metronomeOn.load() || countingIn)
                clickRemaining = (int) (0.02 * rate);
        }
    }
    else
        stepCounter = 0.0;

    // il sequencer batte i suoi colpi prima che il campionatore lavori, cosi'
    // suonano nello stesso blocco
    seq.process (numSamples, tempoAtomic.load(), sampler);

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

    // il compressore sta sull'uscita generale, prima del volume
    if (compressing)
        comp.process (buffer);

    buffer.applyGain (masterGain);

    // RICAMPIONA: il registratore prende quello che esce, non quello che entra
    if (resampling)
        rec.processInput (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), numSamples);
}

//==============================================================================
void Machine::select (PadRef p)
{
    selected = p;
    changed();
}

int Machine::tickForStep (int step) const
{
    const int ticks = seq.quantiseTicks();
    return ((stepOffset + step) * ticks) % juce::jmax (1, seq.length());
}

bool Machine::stepPressed (int step)
{
    const int drum = drumIndex (selected);
    const int ticks = seq.quantiseTicks();
    const int window = juce::jmax (1, ticks / 2 - 1);

    const bool added = seq.toggleHit (drum, tickForStep (step), window,
                                      fullLevel ? 127 : 100);

    setMessage ((added ? "PASSO " : "TOLTO ") + juce::String (stepOffset + step + 1));
    changed();
    return added;
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

    if (eraseMode && seq.isPlaying())
    {
        // CANCELLA mentre gira: toglie i colpi di quel pad dalla sequenza,
        // come sulla macchina vera, invece di buttare via il campione
        seq.eraseDrum (index);
        setMessage ("CANCELLATO " + bankName (p.bank) + juce::String (p.pad));
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

    // nella sequenza finisce il colpo vero, non le pressioni per mutare,
    // cancellare o copiare
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

    // Nella pagina PASSO le quattro file in alto non sono pad: sono i 32
    // passi del suono scelto. Il controllo sta qui, cosi' vale per il Push,
    // per l'OSC e per la finestra senza che nessuno dei tre sappia niente.
    if (stepMode())
        if (const auto cell = cellForPad (p, group))
            if (isStepCell (cell->first))
            {
                stepPressed (stepForCell (cell->first, cell->second));
                return;
            }

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
    if (stepMode())
        if (const auto cell = cellForPad (p, group))
            if (isStepCell (cell->first))
                return;

    const int index = drumIndex (p);
    held[(size_t) index] = false;

    // il pad mollato conta solo per i suoni tenuti: il campionatore sa quali
    sampler.release (index);

    if (onPadPlayed)
        if (const auto cell = cellForPad (p, group))
            onPadPlayed (push2::padNote (cell->first, cell->second), 0);

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
                else if (c.index == 4)
                {
                    resampling = ! resampling;
                    if (resampling)
                        rec.startNow();
                    else
                    {
                        rec.stop();
                        checkRecorder();
                    }
                    setMessage (resampling ? "RICAMPIONA ACCESO" : "RICAMPIONA SPENTO");
                }
                else if (c.index == 5)
                {
                    if (onToLibrary)
                        onToLibrary();
                    setMessage ("IN LIBRERIA");
                }
                else if (c.index == 6)
                    undo();
                else if (c.index == 7)
                    auditionSelected();
                else
                    setMessage ("");
            }
            else if (page == C::pageSequence)
            {
                switch (c.index)
                {
                    case 0: seq.toggleRecord();
                            setMessage (seq.isRecording() ? "REC SEQ ACCESO" : "REC SEQ SPENTO"); break;
                    case 1: seq.toggleOverdub();
                            setMessage (seq.isOverdub() ? "SOVRAINCIDI ACCESO" : "SOVRAINCIDI SPENTO"); break;
                    case 2: seq.setTimingCorrect (! seq.timingCorrect());
                            setMessage (seq.timingCorrect() ? "CORREGGI ACCESO" : "CORREGGI SPENTO"); break;
                    case 3: seq.clearSequence(); setMessage ("CANCELLA SEQ"); break;
                    case 4: seq.newSequence();
                            setMessage ("NUOVA SEQ " + juce::String (seq.sequenceIndex() + 1)); break;
                    case 5: seq.doubleLength(); setMessage ("BATTUTE " + juce::String (seq.bars())); break;
                    case 6: seq.halveLength(); setMessage ("BATTUTE " + juce::String (seq.bars())); break;
                    case 7: // meta', normale, doppia: HALF SPEED e DOUBLE SPEED
                            seq.setSpeed (seq.speed() >= 2.0 ? 0.5
                                                             : (seq.speed() < 1.0 ? 1.0 : 2.0));
                            setMessage ("VELOCITA " + juce::String (seq.speed(), 2)); break;
                    default: break;
                }
            }
            else if (page == C::pageSound)
            {
                if (c.index == 0)
                    auditionSelected();
                else if (c.index == 1)
                {
                    if (onToLibrary)
                        onToLibrary();
                    setMessage ("IN LIBRERIA");
                }
                else if (c.index == 2)
                    undo();
                else if (c.index == 3)
                {
                    auto& d = sampler.drum (drumIndex (selected));
                    d.reverse = ! d.reverse;
                    setMessage (d.reverse ? "ROVESCIO" : "DRITTO");
                }
                else if (c.index == 4)
                    warpSelected();
                else
                    setMessage ("");
            }
            else if (page == C::pageStep)
            {
                if (c.index == 0)
                {
                    seq.eraseDrum (drumIndex (selected));
                    setMessage ("CANCELLA SUONO");
                }
                else if (c.index == 1)
                {
                    stepOffset = juce::jmax (0, stepOffset - numSteps);
                    setMessage ("PASSO " + juce::String (stepOffset + 1));
                }
                else if (c.index == 2)
                {
                    const int total = seq.length() / juce::jmax (1, seq.quantiseTicks());
                    if (stepOffset + numSteps < total)
                        stepOffset += numSteps;
                    setMessage ("PASSO " + juce::String (stepOffset + 1));
                }
                else if (c.index == 3)
                {
                    seq.setTimingCorrect (! seq.timingCorrect());
                    setMessage (seq.timingCorrect() ? "CORREGGI ACCESO" : "CORREGGI SPENTO");
                }
                else
                    setMessage ("");
            }
            else if (page == C::pageMix)
            {
                if (c.index == 0)
                {
                    compressing = ! compressing;
                    setMessage (compressing ? "COMPRESSORE ACCESO" : "COMPRESSORE SPENTO");
                }
                else if (c.index == 1)
                {
                    mutes.fill (false);
                    soloMode = false;
                    setMessage ("TUTTI SUONANO");
                }
                else
                    setMessage ("");
            }
            else if (page == C::pageSong)
            {
                if (c.index == 0)
                {
                    seq.addToSong (seq.sequenceIndex());
                    setMessage ("BRANO " + juce::String (seq.songLength()));
                }
                else if (c.index == 1)
                {
                    seq.removeLastSongStep();
                    setMessage ("BRANO " + juce::String (seq.songLength()));
                }
                else if (c.index == 2)
                {
                    seq.setSongMode (! seq.songMode());
                    setMessage (seq.songMode() ? "BRANO ACCESO" : "BRANO SPENTO");
                }
                else if (c.index == 3)
                {
                    seq.clearSong();
                    setMessage ("BRANO 0");
                }
                else if (c.index == 4)
                {
                    seq.setCountInBars (seq.countInBars() >= 2 ? 0 : seq.countInBars() + 1);
                    setMessage ("AVVIO CONTATO " + juce::String (seq.countInBars()));
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
                else if (c.index == 7)
                    auditionSelected();
                else
                    setMessage ("");
            }
            break;

        case C::quantiseValue:
            quantise = c.index;
            applyQuantiseToSequencer();
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
                soloMode = false;
                setMessage ("TUTTI SUONANO");
                break;
            }

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
            if (shift)
            {
                compressing = ! compressing;
                setMessage (compressing ? "COMPRESSORE ACCESO" : "COMPRESSORE SPENTO");
                break;
            }

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

        case C::play:
            seq.play();
            setMessage ("PLAY");
            break;

        case C::recordSequence:
            if (shift)
            {
                // SHIFT + REC: si campiona, come SAMPLE RECORD sulla macchina
                // vera. Il tasto lampeggia finche' aspetta il suono.
                page = C::pageSample;
                pageName = "CAMPIONA";
                rec.arm();
                setMessage ("ARMATO SOGLIA " + juce::String (rec.threshold()));
            }
            else
            {
                seq.toggleRecord();
                setMessage (seq.isRecording() ? "REC SEQ ACCESO" : "REC SEQ SPENTO");
            }
            break;

        case C::overdub:
            seq.toggleOverdub();
            setMessage (seq.isOverdub() ? "SOVRAINCIDI ACCESO" : "SOVRAINCIDI SPENTO");
            break;

        case C::newSequence:
            seq.newSequence();
            setMessage ("NUOVA SEQ " + juce::String (seq.sequenceIndex() + 1));
            break;

        case C::duplicate:
            // COPIA sul trasporto: la sequenza di adesso finisce in una vuota
            {
                const int from = seq.sequenceIndex();
                int destination = from;
                for (int i = 0; i < Sequencer::numSequences; ++i)
                    if (seq.eventCount (i) == 0)
                    {
                        destination = i;
                        break;
                    }

                seq.copyTo (destination);
                seq.setSequence (destination);
                setMessage ("COPIA SEQ " + juce::String (destination + 1));
            }
            break;

        case C::fixedLength:
            {
                const int next = seq.bars() >= 8 ? 1 : seq.bars() * 2;
                seq.setBars (next);
                setMessage ("BATTUTE " + juce::String (seq.bars()));
            }
            break;

        case C::doubleLength:
            if (shift)
            {
                seq.halveLength();
                setMessage ("BATTUTE " + juce::String (seq.bars()));
            }
            else
            {
                seq.doubleLength();
                setMessage ("RADDOPPIA " + juce::String (seq.bars()));
            }
            break;

        case C::timingCorrect:
            seq.setTimingCorrect (! seq.timingCorrect());
            setMessage (seq.timingCorrect() ? "CORREGGI ACCESO" : "CORREGGI SPENTO");
            break;

        case C::resample:
            resampling = ! resampling;
            if (resampling)
            {
                rec.startNow();
                setMessage ("RICAMPIONA ACCESO");
            }
            else
            {
                rec.stop();
                checkRecorder();
                setMessage ("RICAMPIONA SPENTO");
            }
            break;

        case C::pageLeft:
            if (stepMode())
            {
                stepOffset = juce::jmax (0, stepOffset - numSteps);
                setMessage ("PASSO " + juce::String (stepOffset + 1));
            }
            else
            {
                seq.setSequence (juce::jmax (0, seq.sequenceIndex() - 1));
                setMessage ("SEQUENZA " + juce::String (seq.sequenceIndex() + 1));
            }
            break;

        case C::pageRight:
            if (stepMode())
            {
                const int total = seq.length() / juce::jmax (1, seq.quantiseTicks());
                if (stepOffset + numSteps < total)
                    stepOffset += numSteps;
                setMessage ("PASSO " + juce::String (stepOffset + 1));
            }
            else
            {
                seq.setSequence (juce::jmin (Sequencer::numSequences - 1, seq.sequenceIndex() + 1));
                setMessage ("SEQUENZA " + juce::String (seq.sequenceIndex() + 1));
            }
            break;

        case C::up:    moveSelection (-1, 0); break;
        case C::down:  moveSelection (1, 0);  break;
        case C::left:  moveSelection (0, -1); break;
        case C::right: moveSelection (0, 1);  break;

        case C::recordSample:
            page = C::pageSample;
            pageName = "CAMPIONA";
            rec.arm();
            setMessage ("ARMATO SOGLIA " + juce::String (rec.threshold()));
            break;

        case C::playMode:
            padsIntoSequence = ! padsIntoSequence;
            setMessage (padsIntoSequence ? "SUONA E REGISTRA" : "SUONA SENZA REGISTRARE");
            break;

        case C::sequenceMode:
            page = C::pageSequence;
            pageName = "SEQUENZA";
            setMessage ("SEQUENZE");
            break;

        case C::padSolo:
            soloMode = ! soloMode;
            if (soloMode)
            {
                const int keep = drumIndex (selected);
                for (int i = 0; i < numDrums; ++i)
                    mutes[(size_t) i] = i != keep;
                setMessage ("SOLO QUESTO");
            }
            else
            {
                mutes.fill (false);
                setMessage ("TUTTI SUONANO");
            }
            break;

        case C::padParameters:
            page = C::pageSound;
            pageName = "SUONO";
            setMessage ("PARAMETRI");
            break;

        case C::drumMix:
            page = C::pageMix;
            pageName = "MIX";
            setMessage ("MIXER");
            break;

        case C::sequenceList:
            page = C::pageSequence;
            pageName = "SEQUENZA";
            setMessage ("SEQUENZE");
            break;

        case C::addTrack:
            seq.newSequence();
            setMessage ("NUOVA SEQ " + juce::String (seq.sequenceIndex() + 1));
            break;

        case C::browse:
            if (onBrowseLibrary)
                onBrowseLibrary();
            setMessage ("LIBRERIA");
            break;

        case C::master:
            page = C::pageMix;
            pageName = "MIX";
            setMessage ("GENERALE " + juce::String (juce::roundToInt (masterGain * 100.0f)));
            break;

        case C::setup:
            if (onSetup)
                onSetup();
            setMessage ("SETUP");
            break;

        case C::handBackToLive:
            sampler.stopAll();
            seq.stop();
            setMessage ("A LIVE");
            break;

        case C::stop:
            sampler.stopAll();
            repeatRunning.store (false);
            seq.stop();
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
    // nella pagina MIX i rotativi sono il compressore, il filtro del pad e il
    // volume generale: e' la pagina dove si guarda come suona l'insieme
    if (page == control::Command::pageMix)
    {
        auto& d = sampler.drum (drumIndex (selected));

        switch (which)
        {
            case 0:
                comp.setThresholdDb (comp.thresholdDb() + steps);
                setMessage ("SOGLIA " + juce::String (juce::roundToInt (comp.thresholdDb())));
                break;
            case 1:
                comp.setRatio (comp.ratio() + steps * 0.5);
                setMessage ("RAPPORTO " + juce::String (comp.ratio(), 1));
                break;
            case 2:
                comp.setAttackMs (comp.attackMs() + steps);
                setMessage ("ATTACCO " + juce::String (juce::roundToInt (comp.attackMs())));
                break;
            case 3:
                comp.setReleaseMs (comp.releaseMs() + steps * 10);
                setMessage ("RILASCIO " + juce::String (juce::roundToInt (comp.releaseMs())));
                break;
            case 4:
                d.filter = juce::jlimit (-1.0f, 1.0f, d.filter + steps * 0.05f);
                setMessage ("FILTRO " + juce::String (juce::roundToInt (d.filter * 100.0f)));
                break;
            case 5:
                d.level = juce::jlimit (0.0f, 1.0f, d.level + steps * 0.02f);
                setMessage ("LIVELLO " + juce::String (juce::roundToInt (d.level * 100.0f)));
                break;
            case 6:
                d.pan = juce::jlimit (-1.0f, 1.0f, d.pan + steps * 0.05f);
                setMessage ("PANORAMA " + juce::String (juce::roundToInt (d.pan * 100.0f)));
                break;
            default:
                masterGain = juce::jlimit (0.0f, 1.0f, masterGain + steps * 0.02f);
                setMessage ("GENERALE " + juce::String (juce::roundToInt (masterGain * 100.0f)));
                break;
        }

        changed();
        return;
    }

    // nella pagina FETTE il primo rotativo e' il numero delle fette: si gira e
    // si vedono i tagli muoversi sull'onda, prima di premere RIEMPI
    if (page == control::Command::pageChop && which == 0)
    {
        if (steps != 0)
        {
            slices = slicer::nextCount (slices, steps > 0 ? 1 : -1);
            setMessage ("FETTE " + juce::String (slices));
        }

        return;
    }

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
                // come risponde il pad: una volta, tenuto, a giro, avvia e ferma
                const int modes = 4;
                int which = (int) d.playMode + (steps > 0 ? 1 : -1);
                which = juce::jlimit (0, modes - 1, which);
                d.playMode = (PlayMode) which;
                setMessage ("MODO " + playModeName (d.playMode));
            }
            break;

        case 7:
            // la famiglia del suono, cioe' il colore del pad: il tempo ha il
            // suo rotativo a sinistra, qui serviva piu' questo
            if (steps != 0)
            {
                const int next = juce::jlimit (0, sound::numKinds - 1,
                                               sound::indexOf (d.kind) + (steps > 0 ? 1 : -1));
                d.kind = sound::kindFromIndex (next);
                setMessage ("FAMIGLIA " + sound::name (d.kind));
            }
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
            seq.setSwing (seq.swing() + steps);
            setMessage ("SWING " + juce::String (seq.swing()));
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

    if (stepMode())
        if (const auto cell = cellForPad (p, group))
            if (isStepCell (cell->first))
            {
                const int step = stepForCell (cell->first, cell->second);
                const int ticks = seq.quantiseTicks();
                const int window = juce::jmax (1, ticks / 2 - 1);
                const int tick = ((stepOffset + step) * ticks) % juce::jmax (1, seq.length());

                // il passo dove sta passando la testina si accende come se
                // suonasse, anche quando e' vuoto
                if (seq.isPlaying())
                {
                    const int now = seq.positionTicks() % juce::jmax (1, seq.length());
                    if (std::abs (now - tick) <= window)
                        return PadLight::playing;
                }

                if (tick >= seq.length())
                    return PadLight::off;        // fuori dalla sequenza

                return seq.hasHit (drumIndex (selected), tick, window) ? PadLight::loaded
                                                                       : PadLight::empty;
            }

    if (mutes[(size_t) index])
        return PadLight::muted;
    if (index == drumIndex (selected))
        return PadLight::selected;
    if (sixteenLevels)
        return PadLight::loaded;     // tutto il quadrante suona il suono scelto
    return sampler.drum (index).loaded() ? PadLight::loaded : PadLight::empty;
}

void Machine::setMasterGain (float gain)
{
    masterGain = juce::jlimit (0.0f, 1.0f, gain);
    setMessage ("VOLUME " + juce::String (juce::roundToInt (masterGain * 100.0f)));
    changed();
}

void Machine::warpSelected()
{
    const int index = drumIndex (selected);
    const auto& drum = sampler.drum (index);

    if (! drum.loaded())
    {
        setMessage ("PAD VUOTO");
        changed();
        return;
    }

    const auto source = sampler.copyOf (index);
    const auto name = drum.name;
    const auto kind = drum.kind;
    const double sourceRate = drum.sourceRate;

    int bars = 1;
    const double factor = warp::factorForTempo (source.getNumSamples(), sourceRate, bpm, bars);

    if (std::abs (factor - 1.0) < 0.01)
    {
        setMessage ("GIA' A TEMPO");
        changed();
        return;
    }

    auto stretched = warp::stretch (source, factor);
    if (stretched.getNumSamples() <= 0)
    {
        setMessage ("NON CI RIESCO");
        changed();
        return;
    }

    pushUndo (index, lang::t ("WARP"));
    sampler.loadBuffer (index, name, stretched, sourceRate, kind);

    setMessage ("WARP " + juce::String (bars) + " "
                    + lang::t (bars == 1 ? "BATTUTA" : "BATTUTE"));
    changed();
}

void Machine::auditionSelected()
{
    const int index = drumIndex (selected);
    if (! sampler.drum (index).loaded())
    {
        setMessage ("PAD VUOTO");
        changed();
        return;
    }

    sampler.trigger (index, fullLevel ? 1.0f : 0.9f);
    setMessage ("ASCOLTA " + sampler.drum (index).name);
    changed();
}

Machine::WaveMarkers Machine::waveMarkers() const
{
    using C = control::Command;

    WaveMarkers w;
    w.drumIndex = drumIndex (selected);
    w.sampleVersion = sampler.loadVersion();

    const auto& d = sampler.drum (w.drumIndex);
    const int length = d.sample.getNumSamples();

    // L'onda serve dove si lavora sul campione: quando si ritaglia, quando si
    // fa a fette e quando si e' appena registrato.
    w.show = length > 0 && (page == C::pageSound || page == C::pageChop || page == C::pageSample);
    if (! w.show)
        return w;

    w.kind = d.kind;
    w.startFraction = juce::jlimit (0.0, 1.0, (double) d.start / (double) length);
    w.endFraction = juce::jlimit (w.startFraction, 1.0, (double) d.lastSample() / (double) length);

    // I tagli che RIEMPI farebbe adesso: si vedono prima di premere, e
    // cambiano mentre si gira il numero delle fette
    if (page == C::pageChop)
        for (int i = 1; i < slices; ++i)
            w.slices.push_back ((double) i / (double) slices);

    w.playFraction = sampler.positionOf (w.drumIndex);
    w.caption = juce::String (length / juce::jmax (1.0, d.sourceRate), 2) + "S";
    if (page == C::pageChop)
        w.caption = juce::String (slices) + " " + lang::t ("FETTE") + "  " + w.caption;

    return w;
}

sound::Kind Machine::kindFor (PadRef p) const
{
    // nei passi si vede il colore del suono che si sta scrivendo
    if (stepMode())
        if (const auto cell = cellForPad (p, group))
            if (isStepCell (cell->first))
                return sampler.drum (drumIndex (selected)).kind;

    return sampler.drum (drumIndex (p)).kind;
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
        case C::play:           return seq.isPlaying();
        case C::recordSequence: return seq.isRecording();
        case C::overdub:        return seq.isOverdub();
        case C::timingCorrect:  return seq.timingCorrect();
        case C::padSolo:        return soloMode;
        case C::resample:       return resampling;
        case C::recordSample:   return rec.isWaiting() || rec.isRecording();
        case C::playMode:       return ! padsIntoSequence;
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
    if (! sampler.loadBuffer (index, name, recorded, rate, sound::Kind::registrato))
        return false;

    setMessage ("REGISTRATO " + juce::String (recorded.getNumSamples() / juce::jmax (1.0, rate), 1) + "S");
    changed();
    return true;
}

Machine::ButtonLamp Machine::lampFor (const control::Control& c) const
{
    using C = control::Command;

    const juce::Colour bianco (0xfff3f3f1);
    const juce::Colour verde  (0xff3ee05c);
    const juce::Colour rosso  (0xffff3b30);
    const juce::Colour ambra  (0xffffb020);

    const bool lit = buttonLit (c);

    switch (c.command)
    {
        case C::play:
            // verde fisso mentre gira, smorto da fermo
            return { seq.isPlaying() ? Lamp::acceso : Lamp::smorto, verde };

        case C::stop:
            // bianco pieno quando e' fermo: e' quello il momento in cui serve
            return { seq.isPlaying() ? Lamp::smorto : Lamp::acceso, bianco };

        case C::recordSequence:
            // rosso fisso mentre scrive, lampeggiante quando aspetta: o il
            // suono da campionare, o il via della sequenza
            if (rec.isWaiting())
                return { Lamp::lampeggia, rosso };
            if (rec.isRecording())
                return { Lamp::acceso, rosso };
            if (seq.isRecording())
                return { seq.isPlaying() ? Lamp::acceso : Lamp::lampeggia, rosso };
            return { Lamp::smorto, rosso };

        case C::overdub:
            return { seq.isOverdub() ? Lamp::acceso : Lamp::smorto, ambra };

        case C::recordSample:
            // la registrazione del campione: lampeggia mentre aspetta il suono
            if (rec.isWaiting())
                return { Lamp::lampeggia, rosso };
            return { rec.isRecording() ? Lamp::acceso : Lamp::smorto, rosso };

        case C::resample:
            return { resampling ? Lamp::acceso : Lamp::smorto, rosso };

        case C::metronome:
            return { metronome ? Lamp::acceso : Lamp::smorto, verde };

        case C::padMute:
        case C::padSolo:
        case C::erase:
            return { lit ? Lamp::acceso : Lamp::smorto, ambra };

        default:
            break;
    }

    return { lit ? Lamp::acceso : Lamp::smorto, bianco };
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
        labels.set (4, lang::t ("RICAMPIONA"));
        labels.set (5, lang::t ("IN LIBRERIA"));
        labels.set (6, lang::t ("ANNULLA"));
        labels.set (7, lang::t ("ASCOLTA"));
    }
    else if (page == control::Command::pageSequence)
    {
        labels.set (0, lang::t ("REC SEQ"));
        labels.set (1, lang::t ("SOVRAINCIDI"));
        labels.set (2, lang::t ("CORREGGI"));
        labels.set (3, lang::t ("CANCELLA SEQ"));
        labels.set (4, lang::t ("NUOVA SEQ"));
        labels.set (5, lang::t ("RADDOPPIA"));
        labels.set (6, lang::t ("DIMEZZA"));
        labels.set (7, lang::t ("VELOCITA"));
    }
    else if (page == control::Command::pageSound)
    {
        labels.set (0, lang::t ("ASCOLTA"));
        labels.set (1, lang::t ("IN LIBRERIA"));
        labels.set (2, lang::t ("ANNULLA"));
        labels.set (3, lang::t ("ROVESCIO"));
        labels.set (4, lang::t ("WARP"));
    }
    else if (page == control::Command::pageStep)
    {
        labels.set (0, lang::t ("CANCELLA SUONO"));
        labels.set (1, lang::t ("PASSI -"));
        labels.set (2, lang::t ("PASSI +"));
        labels.set (3, lang::t ("CORREGGI"));
    }
    else if (page == control::Command::pageMix)
    {
        labels.set (0, lang::t ("COMPRESSORE"));
        labels.set (1, lang::t ("TUTTI SUONANO"));
    }
    else if (page == control::Command::pageSong)
    {
        labels.set (0, lang::t ("AGGIUNGI"));
        labels.set (1, lang::t ("TOGLI"));
        labels.set (2, lang::t ("BRANO"));
        labels.set (3, lang::t ("CANCELLA SEQ"));
        labels.set (4, lang::t ("AVVIO CONTATO"));
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
        labels.set (7, lang::t ("ASCOLTA"));
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
        if (sampler.loadBuffer (next, slice.name, slice.audio, slice.sampleRate, slice.kind))
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

void Machine::moveSelection (int rows, int columns)
{
    // si cammina sulla griglia come la si vede: riga 0 in alto
    const auto cell = cellForPad (selected, group);
    if (! cell)
        return;

    const int row = juce::jlimit (0, 7, cell->first + rows);
    const int column = juce::jlimit (0, 7, cell->second + columns);

    select (padForCell (row, column, group));
    setMessage (bankName (selected.bank) + juce::String (selected.pad));
}

void Machine::applyQuantiseToSequencer()
{
    const double beats = quantiseBeats (quantise);
    seq.setQuantiseTicks (juce::jmax (1, (int) std::llround (beats * Sequencer::ticksPerBeat)));
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
        if (sampler.loadBuffer (at, slice.name, slice.audio, slice.sampleRate, slice.kind))
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

    auto list = slicer::cut (source, sourceRate, slices, name, "FETTE", d.kind);
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
    const auto kind = sampler.drum (index).kind;
    pushUndo (index, what);
    if (! sampler.loadBuffer (index, name, buffer, sourceRate, kind))
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
    if (page == control::Command::pageStep)
    {
        lines.add (lang::t ("PASSI") + "|" + juce::String (stepOffset + 1) + "-"
                       + juce::String (stepOffset + numSteps));
        lines.add (lang::t ("PASSO") + "|" + control::quantiseName (quantise));
        lines.add (lang::t ("POSIZIONE") + "|" + juce::String (seq.positionBar()) + "."
                       + juce::String (seq.positionBeat()));
        lines.add (lang::t ("COLPI") + "|" + juce::String (seq.eventCount()));
    }
    else if (page == control::Command::pageMix)
    {
        lines.add (lang::t ("SOGLIA") + "|" + juce::String (juce::roundToInt (comp.thresholdDb())));
        lines.add (lang::t ("RAPPORTO") + "|" + juce::String (comp.ratio(), 1));
        lines.add (lang::t ("ATTACCO") + "|" + juce::String (juce::roundToInt (comp.attackMs())));
        lines.add (lang::t ("RILASCIO") + "|" + juce::String (juce::roundToInt (comp.releaseMs())));
        lines.add (lang::t ("FILTRO") + "|"
                       + juce::String (juce::roundToInt (d.filter * 100.0f)));
        lines.add (lang::t ("LIVELLO") + "|" + juce::String (juce::roundToInt (d.level * 100.0f)));
        lines.add (lang::t ("PANORAMA") + "|" + juce::String (juce::roundToInt (d.pan * 100.0f)));
        lines.add (lang::t ("GENERALE") + "|" + juce::String (juce::roundToInt (masterGain * 100.0f)));
        return lines;
    }
    else if (page == control::Command::pageSong)
    {
        lines.add (lang::t ("BRANO") + "|" + juce::String (seq.songPosition() + 1) + "/"
                       + juce::String (seq.songLength()));
        lines.add (lang::t ("SEQUENZA") + "|" + juce::String (seq.sequenceIndex() + 1));
        lines.add (lang::t ("AVVIO CONTATO") + "|" + juce::String (seq.countInBars()));
        lines.add (lang::t ("POSIZIONE") + "|" + juce::String (seq.positionBar()) + "."
                       + juce::String (seq.positionBeat()));
    }
    else if (page == control::Command::pageSequence)
    {
        lines.add (lang::t ("SEQUENZA") + "|" + juce::String (seq.sequenceIndex() + 1));
        lines.add (lang::t ("BATTUTE") + "|" + juce::String (seq.bars()));
        lines.add (lang::t ("POSIZIONE") + "|" + juce::String (seq.positionBar()) + "."
                       + juce::String (seq.positionBeat()));
        lines.add (lang::t ("COLPI") + "|" + juce::String (seq.eventCount()));
    }
    else if (page == control::Command::pageSample)
    {
        lines.add (lang::t ("SOGLIA") + "|" + juce::String (rec.threshold()));
        lines.add (lang::t ("DURATA") + "|" + juce::String (rec.lengthSeconds(), 1) + "S");
        lines.add (lang::t ("ANTICIPO") + "|"
                       + juce::String (juce::roundToInt (rec.preRollSeconds() * 1000.0)) + "MS");
        lines.add (lang::t ("INGRESSO") + "|"
                       + juce::String (juce::roundToInt (rec.inputLevel() * 100.0f)));
    }
    else if (page == control::Command::pageSound)
    {
        // nella pagina SUONO i campi sono gli otto rotativi, in ordine
        lines.add (lang::t ("LIVELLO") + "|" + juce::String (juce::roundToInt (d.level * 100.0f)));
        lines.add (lang::t ("ACCORDA") + "|" + juce::String (d.tuneSemitones > 0 ? "+" : "")
                       + juce::String (d.tuneSemitones));
        lines.add (lang::t ("PANORAMA") + "|" + juce::String (juce::roundToInt (d.pan * 100.0f)));
        lines.add (lang::t ("INIZIO") + "|" + juce::String (d.start));
        lines.add (lang::t ("FINE") + "|" + juce::String (d.lastSample()));
        lines.add (lang::t ("GRUPPO") + "|" + juce::String (d.muteGroup));
        lines.add (lang::t ("MODO") + "|" + lang::t (playModeName (d.playMode)));
        lines.add (lang::t ("FAMIGLIA") + "|" + lang::t (sound::name (d.kind)));
        return lines;
    }
    else
    {
        lines.add (lang::t ("LIVELLO") + "|" + juce::String (juce::roundToInt (d.level * 100.0f)));
        lines.add (lang::t ("ACCORDA") + "|" + juce::String (d.tuneSemitones > 0 ? "+" : "")
                       + juce::String (d.tuneSemitones));
        lines.add (lang::t ("PANORAMA") + "|" + juce::String (juce::roundToInt (d.pan * 100.0f)));
        lines.add (lang::t ("MODO") + "|" + lang::t (playModeName (d.playMode)));
    }
    lines.add (lang::t ("TEMPO") + "|" + juce::String (juce::roundToInt (bpm)));
    lines.add (lang::t ("STATO") + "|" + lang::phrase (message.isEmpty() ? juce::String ("PRONTO") : message));
    return lines;
}

} // namespace machine
