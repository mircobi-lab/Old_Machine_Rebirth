#include "Sequencer.h"

void Sequencer::prepare (double sampleRate)
{
    rate = sampleRate > 0 ? sampleRate : 44100.0;
    ticks = 0.0;
    shownTicks.store (0.0);
    playing.store (false);
    recording.store (false);
    overdub.store (false);
}

int Sequencer::bars() const
{
    return sequences[current].barCount.load();
}

void Sequencer::setBars (int count)
{
    sequences[current].barCount.store (juce::jlimit (1, maxBars, count));
}

int Sequencer::eventCount() const
{
    return eventCount (current);
}

int Sequencer::eventCount (int sequence) const
{
    if (! juce::isPositiveAndBelow (sequence, numSequences))
        return 0;

    return sequences[sequence].count.load();
}

int Sequencer::lengthInTicks() const
{
    return juce::jmax (ticksPerBar, bars() * ticksPerBar);
}

double Sequencer::positionInBeats() const
{
    return shownTicks.load() / (double) ticksPerBeat;
}

int Sequencer::positionBar() const
{
    return (int) (shownTicks.load() / (double) ticksPerBar) + 1;
}

int Sequencer::positionBeat() const
{
    const auto inBar = std::fmod (shownTicks.load(), (double) ticksPerBar);
    return (int) (inBar / (double) ticksPerBeat) + 1;
}

void Sequencer::setQuantiseTicks (int t)
{
    quantise = juce::jlimit (1, ticksPerBar, t);
}

void Sequencer::setSpeed (double factor)
{
    speedFactor = juce::jlimit (0.25, 4.0, factor);
}

void Sequencer::setSwing (int percent)
{
    swingPercent = juce::jlimit (50, 75, percent);
}

void Sequencer::setSequence (int index)
{
    if (! juce::isPositiveAndBelow (index, numSequences))
        return;

    editing.store (true);
    current = index;
    ticks = 0.0;
    shownTicks.store (0.0);
    editing.store (false);
}

void Sequencer::setCountInBars (int bars)
{
    countIn = juce::jlimit (0, 2, bars);
}

void Sequencer::play()
{
    // con l'avvio contato il metronomo batte una o due battute a vuoto prima
    // che parta davvero: serve per entrare a tempo quando si registra
    if (countIn > 0 && ! playing.load())
    {
        countTicks = 0.0;
        counting.store (true);
    }

    playing.store (true);
    stopPressedOnce.store (false);
}

void Sequencer::addToSong (int sequence, int repeats)
{
    if (songSteps >= maxSongSteps || ! juce::isPositiveAndBelow (sequence, numSequences))
        return;

    songList[(size_t) songSteps] = { sequence, juce::jlimit (1, 99, repeats) };
    ++songSteps;
}

void Sequencer::removeLastSongStep()
{
    if (songSteps > 0)
        --songSteps;

    if (songAt.load() >= songSteps)
        songAt.store (0);
}

void Sequencer::clearSong()
{
    songSteps = 0;
    songAt.store (0);
    songRepeat = 0;
}

Sequencer::SongStep Sequencer::songStep (int index) const
{
    if (! juce::isPositiveAndBelow (index, songSteps))
        return {};

    return songList[(size_t) index];
}

void Sequencer::setSongMode (bool on)
{
    song.store (on && songSteps > 0);
    songAt.store (0);
    songRepeat = 0;

    if (song.load())
        setSequence (songList[0].sequence);
}

void Sequencer::stop()
{
    if (playing.load())
    {
        playing.store (false);
        recording.store (false);
        overdub.store (false);
        stopPressedOnce.store (true);
        return;
    }

    // fermo e si premo di nuovo: si torna all'inizio, come sulla macchina vera
    rewind();
}

void Sequencer::rewind()
{
    editing.store (true);
    ticks = 0.0;
    shownTicks.store (0.0);
    editing.store (false);
    stopPressedOnce.store (false);
}

void Sequencer::toggleRecord()
{
    const bool on = ! recording.load();
    recording.store (on);
    if (on)
    {
        overdub.store (false);
        play();            // che passa anche dall'avvio contato, se c'e'
    }
}

void Sequencer::toggleOverdub()
{
    const bool on = ! overdub.load();
    overdub.store (on);
    if (on)
    {
        recording.store (true);
        play();
    }
}

int Sequencer::quantised (int tick) const
{
    if (! correct.load() || quantise <= 1)
        return tick;

    const int step = quantise;
    const int snapped = ((tick + step / 2) / step) * step;
    return snapped % lengthInTicks();
}

void Sequencer::recordHit (int drum, int velocity)
{
    if (! recording.load() || ! playing.load() || counting.load())
        return;

    auto& sequence = sequences[current];
    const int count = sequence.count.load();
    if (count >= maxEvents)
        return;

    Event e;
    e.tick = quantised ((int) shownTicks.load() % lengthInTicks());
    e.drum = drum;
    e.velocity = juce::jlimit (1, 127, velocity);

    // si scrive prima l'evento e solo dopo si alza il contatore: fino a quel
    // momento il filo dell'audio non lo vede nemmeno
    sequence.events[(size_t) count] = e;
    sequence.count.store (count + 1);
}

void Sequencer::eraseDrum (int drum)
{
    auto& sequence = sequences[current];

    editing.store (true);

    int kept = 0;
    const int count = sequence.count.load();
    for (int i = 0; i < count; ++i)
        if (sequence.events[(size_t) i].drum != drum)
            sequence.events[(size_t) kept++] = sequence.events[(size_t) i];

    sequence.count.store (kept);
    editing.store (false);
}

bool Sequencer::hasHit (int drum, int tick, int window) const
{
    return velocityAt (drum, tick, window) > 0;
}

int Sequencer::velocityAt (int drum, int tick, int window) const
{
    const auto& sequence = sequences[current];
    const int count = sequence.count.load();
    const int length = lengthInTicks();

    for (int i = 0; i < count; ++i)
    {
        const auto& e = sequence.events[(size_t) i];
        if (e.drum != drum || e.velocity <= 0)
            continue;

        // la distanza si misura sul giro: il tick 0 e l'ultimo sono vicini
        int distance = std::abs (e.tick % length - tick % length);
        distance = juce::jmin (distance, length - distance);

        if (distance <= window)
            return e.velocity;
    }

    return 0;
}

void Sequencer::setHit (int drum, int tick, int velocity)
{
    auto& sequence = sequences[current];
    const int count = sequence.count.load();
    if (count >= maxEvents)
        return;

    Event e;
    e.tick = tick % lengthInTicks();
    e.drum = drum;
    e.velocity = juce::jlimit (1, 127, velocity);

    sequence.events[(size_t) count] = e;
    sequence.count.store (count + 1);
}

void Sequencer::removeHit (int drum, int tick, int window)
{
    auto& sequence = sequences[current];
    const int length = lengthInTicks();

    editing.store (true);

    int kept = 0;
    const int count = sequence.count.load();
    for (int i = 0; i < count; ++i)
    {
        const auto& e = sequence.events[(size_t) i];

        bool remove = e.drum == drum;
        if (remove)
        {
            int distance = std::abs (e.tick % length - tick % length);
            distance = juce::jmin (distance, length - distance);
            remove = distance <= window;
        }

        if (! remove)
            sequence.events[(size_t) kept++] = e;
    }

    sequence.count.store (kept);
    editing.store (false);
}

bool Sequencer::toggleHit (int drum, int tick, int window, int velocity)
{
    if (hasHit (drum, tick, window))
    {
        removeHit (drum, tick, window);
        return false;
    }

    setHit (drum, tick, velocity);
    return true;
}

void Sequencer::clearSequence()
{
    editing.store (true);
    sequences[current].count.store (0);
    editing.store (false);
}

void Sequencer::newSequence()
{
    for (int i = 0; i < numSequences; ++i)
        if (sequences[i].count.load() == 0)
        {
            setSequence (i);
            return;
        }

    // tutte piene: si pulisce quella che segue
    setSequence ((current + 1) % numSequences);
    clearSequence();
}

void Sequencer::copyTo (int destination)
{
    if (! juce::isPositiveAndBelow (destination, numSequences) || destination == current)
        return;

    const auto& from = sequences[current];
    auto& to = sequences[destination];
    const int count = from.count.load();

    editing.store (true);
    for (int i = 0; i < count; ++i)
        to.events[(size_t) i] = from.events[(size_t) i];

    to.barCount.store (from.barCount.load());
    to.count.store (count);
    editing.store (false);
}

void Sequencer::doubleLength()
{
    auto& sequence = sequences[current];
    const int count = sequence.count.load();
    const int length = lengthInTicks();
    const int newBars = juce::jmin (maxBars, bars() * 2);

    if (newBars == bars() || count * 2 > maxEvents)
        return;

    editing.store (true);

    for (int i = 0; i < count; ++i)
    {
        auto copy = sequence.events[(size_t) i];
        copy.tick += length;
        sequence.events[(size_t) (count + i)] = copy;
    }

    sequence.barCount.store (newBars);
    sequence.count.store (count * 2);
    editing.store (false);
}

void Sequencer::halveLength()
{
    auto& sequence = sequences[current];
    const int newBars = juce::jmax (1, bars() / 2);
    if (newBars == bars())
        return;

    const int length = newBars * ticksPerBar;

    editing.store (true);

    int kept = 0;
    const int count = sequence.count.load();
    for (int i = 0; i < count; ++i)
        if (sequence.events[(size_t) i].tick < length)
            sequence.events[(size_t) kept++] = sequence.events[(size_t) i];

    sequence.barCount.store (newBars);
    sequence.count.store (kept);
    editing.store (false);
}

int Sequencer::swungTick (int tick) const
{
    if (swingPercent <= 50)
        return tick;

    // lo swing sposta in avanti i sedicesimi dispari
    const int step = ticksPerBeat / 4;
    const int which = tick / step;
    if (which % 2 == 0)
        return tick;

    const double shift = (double) step * (double) (swingPercent - 50) / 50.0;
    return tick + (int) shift;
}

void Sequencer::fire (SampleEngine& engine, int fromTick, int toTick)
{
    auto& sequence = sequences[current];
    const int count = sequence.count.load();
    const int length = lengthInTicks();

    for (int i = 0; i < count; ++i)
    {
        const auto& e = sequence.events[(size_t) i];
        if (e.velocity <= 0)
            continue;

        int at = swungTick (e.tick) % length;

        const bool due = fromTick <= toTick ? (at >= fromTick && at < toTick)
                                            : (at >= fromTick || at < toTick);   // giro finito
        if (! due)
            continue;

        engine.trigger (e.drum, (float) e.velocity / 127.0f);
        triggered.store (e.drum);
    }
}

void Sequencer::process (int numSamples, double bpm, SampleEngine& engine)
{
    if (numSamples <= 0)
        return;

    if (! playing.load() || editing.load())
        return;

    const double beatsPerSecond = juce::jmax (20.0, bpm) / 60.0;
    const double ticksPerSample = beatsPerSecond * (double) ticksPerBeat * speedFactor / rate;

    // l'avvio contato: si conta e basta, niente colpi e niente registrazione
    if (counting.load())
    {
        countTicks += ticksPerSample * numSamples;
        if (countTicks < (double) (countIn * ticksPerBar))
            return;

        counting.store (false);
        ticks = 0.0;
    }

    const int length = lengthInTicks();

    const int from = (int) ticks % length;
    ticks += ticksPerSample * numSamples;

    bool wrapped = false;
    while (ticks >= length)
    {
        ticks -= length;
        wrapped = true;
    }

    const int to = (int) ticks % length;

    shownTicks.store (ticks);

    if (from != to)
        fire (engine, from, to);

    // finito il giro, nel brano si passa al pezzo dopo
    if (wrapped && song.load() && songSteps > 0)
    {
        ++songRepeat;

        const int at = songAt.load();
        if (songRepeat >= songList[(size_t) at].repeats)
        {
            songRepeat = 0;
            const int next = (at + 1) % songSteps;
            songAt.store (next);

            // il cambio di sequenza qui dentro non alloca: sposta un indice
            current = songList[(size_t) next].sequence;
        }
    }
}
