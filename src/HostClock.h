/*
 * Host-driven 24 PPQ clock. No allocation, locks or host API calls.
 * Copyright 2026 B-Step contributors. Distributed under the project license.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace bstep
{
struct HostPosition
{
    std::int64_t samples = 0;
    double quarterNotes = 0;
    double bpm = 120;
    double sampleRate = 48000;
    double speed = 1;
    int blockSize = 0;
    bool playing = false;
    bool looping = false;
    double loopStart = 0;
    double loopEnd = 0;
};

enum class HostTransport
{
    start,
    stop,
    seek,
    loop,
    invalid
};

class HostClock
{
  public:
    // Emit the current clock + 1, as before: B-Step first sends the messages
    // prepared by the previous clock and then prepares the next clock's events.
    template <typename Transport, typename Tick>
    void process(const HostPosition &position, Transport transport, Tick tick)
    {
        if (!valid(position))
        {
            if (running)
                transport(HostTransport::invalid, previous.samples);
            running = false;
            initialized = false;
            haveClock = false;
            return;
        }

        if (!position.playing)
        {
            if (running)
                transport(HostTransport::stop, position.samples);
            else if (initialized && position.samples != previous.samples)
                transport(HostTransport::seek, position.samples);
            running = false;
            previous = position;
            initialized = true;
            return;
        }

        if (!running)
        {
            haveClock = false;
            transport(HostTransport::start, position.samples);
        }
        else
        {
            const double expected = previous.quarterNotes + quarterNotesPerBlock(previous);
            const double difference = position.quarterNotes - expected;
            const double tolerance = std::max(1.0e-8, 2 * quarterNotesPerSample(position));
            const bool speedChanged = position.speed != previous.speed;
            const bool continuousBeats = std::abs(difference) <= tolerance;
            const bool loopWrap = position.looping && position.loopEnd > position.loopStart &&
                                  previous.quarterNotes < position.loopEnd + tolerance &&
                                  expected >= position.loopEnd - tolerance &&
                                  position.quarterNotes < previous.quarterNotes;

            if (speedChanged || loopWrap || !continuousBeats)
            {
                haveClock = false;
                transport(loopWrap ? HostTransport::loop : HostTransport::seek, position.samples);
            }
        }

        const double start = position.quarterNotes;
        const double perSample = quarterNotesPerSample(position);
        const double clocksPerQuarter = 24.0 / position.speed;
        for (int offset = 0; offset < position.blockSize; ++offset)
        {
            const double quarter = start + offset * perSample;
            // The sequencer's absolute-step code expects nonnegative clocks.
            // Signed musical time allows a preroll block to cross zero safely.
            if (quarter < 0)
                continue;
            const double absolute = quarter * clocksPerQuarter;
            if (absolute >= static_cast<double>(std::numeric_limits<std::int64_t>::max() - 1))
                continue;
            const auto clock = static_cast<std::int64_t>(std::floor(absolute + 1.0e-9)) + 1;
            if (!haveClock || clock > lastClock)
            {
                tick(offset, clock);
                lastClock = clock;
                haveClock = true;
            }
        }
        previous = position;
        initialized = running = true;
    }

  private:
    static bool valid(const HostPosition &position)
    {
        return position.blockSize > 0 && std::isfinite(position.quarterNotes) &&
               std::isfinite(position.bpm) && position.bpm > 0 &&
               std::isfinite(position.sampleRate) && position.sampleRate > 0 &&
               std::isfinite(position.speed) && position.speed > 0;
    }

    static double quarterNotesPerSample(const HostPosition &position)
    {
        return position.bpm / (60.0 * position.sampleRate);
    }

    static double quarterNotesPerBlock(const HostPosition &position)
    {
        return position.blockSize * quarterNotesPerSample(position);
    }

    HostPosition previous;
    std::int64_t lastClock = 0;
    bool initialized = false;
    bool running = false;
    bool haveClock = false;
};
} // namespace bstep
