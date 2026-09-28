#pragma once

// Optional test-build instrumentation. Audio callbacks only copy into a bounded
// single-producer ring; a message-thread timer owns all formatting and file I/O.
#include "HostClock.h"
#include <array>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <juce_events/juce_events.h>

namespace bstep
{
class HostClockTrace : private juce::Timer
{
  public:
    HostClockTrace()
    {
        const auto *path = std::getenv("BSTEP_CLOCK_TRACE_DIR");
        if (path == nullptr)
            return;
        const juce::File directory(path);
        if (!directory.isDirectory())
            return;
        static std::atomic<unsigned> nextId{0};
        const auto file = directory.getChildFile("clock-" + juce::String(nextId++) + ".csv");
        output.open(file.getFullPathName().toStdString());
        if (!output)
            return;
        output.precision(17);
        output << "block,event,samples,quarter_notes,bpm,block_size,sample_rate,speed,looping,"
                  "offset,clock_or_midi\n";
        enabled = true;
        startTimer(100);
    }

    ~HostClockTrace() override
    {
        stopTimer();
        drain();
    }

    void beginBlock(const HostPosition &position)
    {
        current = position;
        ++block;
    }

    void record(int event, int offset, std::int64_t number)
    {
        if (!enabled)
            return;
        const auto write = written.load(std::memory_order_relaxed);
        const auto next = (write + 1) % records.size();
        if (next == read.load(std::memory_order_acquire))
        {
            dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        records[write] = {current, block, event, offset, number};
        written.store(next, std::memory_order_release);
    }

  private:
    struct Record
    {
        HostPosition position;
        std::uint64_t block;
        int event;
        int offset;
        std::int64_t number;
    };

    void timerCallback() override { drain(); }

    void drain()
    {
        if (!enabled)
            return;
        auto index = read.load(std::memory_order_relaxed);
        const auto end = written.load(std::memory_order_acquire);
        while (index != end)
        {
            const auto &row = records[index];
            const auto &p = row.position;
            output << row.block << ',' << row.event << ',' << p.samples << ',' << p.quarterNotes
                   << ',' << p.bpm << ',' << p.blockSize << ',' << p.sampleRate << ',' << p.speed
                   << ',' << p.looping << ',' << row.offset << ',' << row.number << '\n';
            index = (index + 1) % records.size();
        }
        read.store(index, std::memory_order_release);
        const auto lost = dropped.exchange(0);
        if (lost)
            output << "dropped," << lost << '\n';
        output.flush();
    }

    std::array<Record, 4096> records;
    std::atomic<std::size_t> read{0}, written{0};
    std::atomic<unsigned> dropped{0};
    HostPosition current;
    std::uint64_t block = 0;
    std::ofstream output;
    bool enabled = false;
};
} // namespace bstep
