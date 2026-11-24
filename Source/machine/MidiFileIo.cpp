#include "MidiFileIo.h"

namespace midifile
{

namespace
{
constexpr int firstNote = 35;       // la nota del primo pad, come sulla macchina vera
}

int drumForNote (int note)
{
    const int drum = note - firstNote;
    return juce::isPositiveAndBelow (drum, machine::numDrums) ? drum : -1;
}

int noteForDrum (int drum)
{
    return firstNote + juce::jlimit (0, machine::numDrums - 1, drum);
}

Result import (const juce::File& file, machine::Machine& m)
{
    Result result;

    juce::FileInputStream stream (file);
    if (! stream.openedOk())
    {
        result.error = "NON LEGGO QUEL FILE";
        return result;
    }

    juce::MidiFile midi;
    if (! midi.readFrom (stream))
    {
        result.error = "NON E' UN FILE MIDI";
        return result;
    }

    // tutto in quarti, cosi' non importa come il file conta il tempo
    midi.convertTimestampTicksToSeconds();

    // il tempo scritto nel file, se c'e'
    juce::MidiMessageSequence tempos;
    midi.findAllTempoEvents (tempos);

    double bpm = m.tempo();
    for (int i = 0; i < tempos.getNumEvents(); ++i)
    {
        const auto& message = tempos.getEventPointer (i)->message;
        if (message.isTempoMetaEvent())
        {
            const double secondsPerQuarter = message.getTempoSecondsPerQuarterNote();
            if (secondsPerQuarter > 0.0)
            {
                bpm = juce::jlimit (30.0, 300.0, 60.0 / secondsPerQuarter);
                break;
            }
        }
    }

    result.bpm = bpm;

    auto& seq = m.sequencer();
    seq.clearSequence();

    const double secondsPerBeat = 60.0 / bpm;
    double lastBeat = 0.0;

    struct Hit { double beat; int drum; int velocity; };
    std::vector<Hit> hits;

    for (int track = 0; track < midi.getNumTracks(); ++track)
    {
        const auto* events = midi.getTrack (track);
        if (events == nullptr)
            continue;

        for (int i = 0; i < events->getNumEvents(); ++i)
        {
            const auto& message = events->getEventPointer (i)->message;
            if (! message.isNoteOn() || message.getVelocity() == 0)
                continue;

            const int drum = drumForNote (message.getNoteNumber());
            if (drum < 0)
                continue;

            const double beat = message.getTimeStamp() / secondsPerBeat;
            lastBeat = juce::jmax (lastBeat, beat);
            hits.push_back ({ beat, drum, (int) message.getVelocity() });
        }
    }

    if (hits.empty())
    {
        result.error = "NESSUNA NOTA SUI PAD";
        return result;
    }

    // le battute che servono per contenerlo tutto
    const int bars = juce::jlimit (1, Sequencer::maxBars,
                                   (int) std::ceil ((lastBeat + 0.001) / Sequencer::beatsPerBar));
    seq.setBars (bars);
    m.setTempoFromOutside (bpm);

    const int length = seq.length();

    for (const auto& hit : hits)
    {
        const int tick = ((int) std::llround (hit.beat * Sequencer::ticksPerBeat)) % length;
        seq.setHit (hit.drum, tick, hit.velocity);
        ++result.notes;
    }

    result.bars = bars;
    return result;
}

bool exportSequence (const juce::File& file, machine::Machine& m)
{
    auto& seq = m.sequencer();
    if (seq.eventCount() <= 0)
        return false;

    juce::MidiMessageSequence track;

    // il tempo in testa, se no chi lo apre non sa a che velocita' va
    track.addEvent (juce::MidiMessage::tempoMetaEvent (
                        (int) std::llround (60.0e6 / juce::jmax (20.0, m.tempo()))));

    for (int i = 0; i < seq.eventCount(); ++i)
    {
        const auto event = seq.eventAt (i);
        if (event.velocity <= 0)
            continue;

        const int note = noteForDrum (event.drum);

        // i tempi di un file MIDI si contano in passi, non in quarti, e il
        // passo qui e' lo stesso del nostro sequencer
        const double at = (double) event.tick;

        track.addEvent (juce::MidiMessage::noteOn (16, note, (juce::uint8) event.velocity), at);
        track.addEvent (juce::MidiMessage::noteOff (16, note), at + Sequencer::ticksPerBeat / 8.0);
    }

    track.updateMatchedPairs();

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (Sequencer::ticksPerBeat);
    midi.addTrack (track);

    file.deleteFile();
    juce::FileOutputStream stream (file);
    if (! stream.openedOk())
        return false;

    return midi.writeTo (stream);
}

} // namespace midifile
