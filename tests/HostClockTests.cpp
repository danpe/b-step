#include "HostClock.h"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using bstep::HostClock;
using bstep::HostPosition;
using bstep::HostTransport;

#define CHECK(condition)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if (!(condition))                                                                          \
            throw std::runtime_error(std::string(__func__) + ":" + std::to_string(__LINE__) +      \
                                     " " #condition);                                              \
    } while (false)

struct Runner
{
    HostClock clock;
    std::vector<std::pair<std::int64_t, std::int64_t>> ticks;
    std::vector<HostTransport> transport;
    std::int64_t elapsed = 0;
    void process(HostPosition position)
    {
        clock.process(
            position, [&](HostTransport event, std::int64_t) { transport.push_back(event); },
            [&](int offset, std::int64_t number) {
                CHECK(offset >= 0 && offset < position.blockSize);
                ticks.emplace_back(elapsed + offset, number);
            });
        elapsed += position.blockSize;
    }
};

HostPosition playing()
{
    HostPosition position;
    position.playing = true;
    position.blockSize = 1024;
    return position;
}

void advance(HostPosition &position)
{
    position.samples += position.blockSize;
    position.quarterNotes += position.blockSize * position.bpm / (60 * position.sampleRate);
}

void constantTempo()
{
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (double bpm : {60.0, 120.0, 121.5, 187.0})
            for (double speed : {0.25, 0.5, 1.0, 2.0, 4.0})
                for (int size : {64, 127, 1024, 4096})
                {
                    Runner runner;
                    auto position = playing();
                    position.sampleRate = rate;
                    position.bpm = bpm;
                    position.speed = speed;
                    position.blockSize = size;
                    for (int block = 0; block < 40; ++block)
                    {
                        runner.process(position);
                        advance(position);
                    }
                    CHECK(runner.transport == std::vector<HostTransport>{HostTransport::start});
                    const double interval = rate * 60 * speed / (bpm * 24);
                    for (std::size_t i = 0; i < runner.ticks.size(); ++i)
                    {
                        CHECK(runner.ticks[i].second == static_cast<std::int64_t>(i + 1));
                        CHECK(std::abs(runner.ticks[i].first - std::ceil(i * interval - 1e-8)) <=
                              1);
                    }
                }
}

void musicalHistory()
{
    Runner runner;
    auto position = playing();
    position.samples = 48000 * 600;
    position.quarterNotes = 317.25; // Earlier sections used different tempos.
    runner.process(position);
    CHECK(runner.ticks.front().second == static_cast<std::int64_t>(317.25 * 24) + 1);
}

void continuousTempoChanges()
{
    Runner runner;
    auto position = playing();
    position.samples = 48000LL * 60 * 60 * 6;
    position.quarterNotes = position.samples * 120.0 / (48000 * 60);
    for (int block = 0; block < 600; ++block)
    {
        position.bpm = block % 4 == 0 ? 60 : block % 4 == 1 ? 121.5 : block % 4 == 2 ? 187 : 80;
        runner.process(position);
        advance(position);
    }
    CHECK(runner.transport == std::vector<HostTransport>{HostTransport::start});
    for (std::size_t i = 1; i < runner.ticks.size(); ++i)
        CHECK(runner.ticks[i].second == runner.ticks[i - 1].second + 1);
}

void capturedReaperTempoChanges()
{
    std::ifstream file(std::string(BSTEP_TEST_FIXTURES) + "/reaper-7.80-realtime-tempo.csv");
    CHECK(file.good());
    std::string row;
    std::getline(file, row);
    std::array<Runner, 7> runners;
    int count = 0, tempoEdits = 0;
    double previousBpm = 120;
    double expectedQuarter = 0;
    while (std::getline(file, row))
    {
        std::replace(row.begin(), row.end(), ',', ' ');
        std::istringstream fields(row);
        double seconds, quarter, bpm, state, size, rate;
        CHECK(static_cast<bool>(fields >> seconds >> quarter >> bpm >> state >> size >> rate));
        auto position = playing();
        position.samples = static_cast<std::int64_t>(std::llround(seconds * rate));
        position.quarterNotes = quarter;
        position.bpm = bpm;
        position.sampleRate = rate;
        position.blockSize = static_cast<int>(size);
        if (count == 0)
            expectedQuarter = quarter;
        for (auto &runner : runners)
            runner.process(position);
        if (bpm != previousBpm)
            ++tempoEdits;
        previousBpm = bpm;
        CHECK(std::abs(quarter - expectedQuarter) <= 2 * bpm / (60 * rate));
        expectedQuarter = quarter + size * bpm / (60 * rate);
        ++count;
    }
    CHECK(count > 200);
    CHECK(tempoEdits == 47);
    CHECK(runners[0].transport == std::vector<HostTransport>{HostTransport::start});
    for (std::size_t i = 1; i < runners[0].ticks.size(); ++i)
        CHECK(runners[0].ticks[i].second == runners[0].ticks[i - 1].second + 1);
    for (const auto &runner : runners)
        CHECK(runner.ticks == runners[0].ticks);
    std::cout << "REAPER capture: " << count << " blocks, " << tempoEdits
              << " tempo edits, 0 jumps; seven instances match\n";
}

void seeksLoopsAndSpeed()
{
    Runner runner;
    auto position = playing();
    runner.process(position);
    position.samples = 48000 * 10;
    position.quarterNotes = 20;
    runner.process(position);
    CHECK(runner.transport.back() == HostTransport::seek);
    position.samples = 0;
    position.quarterNotes = 0;
    runner.process(position);
    CHECK(runner.transport.back() == HostTransport::seek);
    advance(position);
    position.speed = 2;
    runner.process(position);
    CHECK(runner.transport.back() == HostTransport::seek);

    Runner loop;
    position = playing();
    position.quarterNotes = 3.98;
    position.samples = static_cast<std::int64_t>(3.98 * 24000);
    position.looping = true;
    position.loopEnd = 4;
    loop.process(position);
    position.quarterNotes = 0.0226666666666667;
    position.samples = 544;
    loop.process(position);
    CHECK(loop.transport.back() == HostTransport::loop);
    position.quarterNotes = 2;
    position.samples = 48000;
    loop.process(position);
    CHECK(loop.transport.back() == HostTransport::seek); // Loop mode is not itself a wrap.

    Runner combined;
    position = playing();
    combined.process(position);
    advance(position);
    position.bpm = 121;
    position.quarterNotes += 4;
    position.samples = static_cast<std::int64_t>(position.quarterNotes * 60 * 48000 / 121);
    combined.process(position);
    CHECK(combined.transport.back() == HostTransport::seek);
}

void stoppedAndInvalid()
{
    Runner runner;
    auto position = playing();
    position.playing = false;
    runner.process(position);
    position.bpm = 180;
    runner.process(position);
    CHECK(runner.transport.empty());
    CHECK(runner.ticks.empty());
    position.playing = true;
    runner.process(position);
    CHECK(runner.transport.back() == HostTransport::start);
    advance(position);
    position.playing = false;
    runner.process(position);
    CHECK(runner.transport.back() == HostTransport::stop);
    position.playing = true;
    runner.process(position);
    CHECK(runner.transport.back() == HostTransport::start);
    position.bpm = std::numeric_limits<double>::quiet_NaN();
    runner.process(position);
    CHECK(runner.transport.back() == HostTransport::invalid);
    const auto count = runner.transport.size();
    runner.process(position);
    CHECK(runner.transport.size() == count);
    position.bpm = 120;
    runner.process(position);
    CHECK(runner.transport.back() == HostTransport::start);
}

void negativePreroll()
{
    Runner runner;
    auto position = playing();
    position.samples = -512;
    position.quarterNotes = -512.0 / 24000;
    runner.process(position);
    CHECK(!runner.ticks.empty());
    CHECK(runner.ticks.front().first == 512);
    CHECK(runner.ticks.front().second == 1);
}

void boundaryJitterDoesNotDuplicate()
{
    Runner runner;
    auto position = playing();
    position.blockSize = 1000;
    for (int i = 0; i < 50; ++i)
    {
        position.samples = i * 1000;
        position.quarterNotes = (position.samples - (i % 2 ? 1.0 : 0.0)) / 24000;
        runner.process(position);
    }
    CHECK(runner.transport.size() == 1);
    for (std::size_t i = 1; i < runner.ticks.size(); ++i)
        CHECK(runner.ticks[i].second == runner.ticks[i - 1].second + 1);
}

int main()
{
    try
    {
        constantTempo();
        musicalHistory();
        continuousTempoChanges();
        capturedReaperTempoChanges();
        seeksLoopsAndSpeed();
        stoppedAndInvalid();
        negativePreroll();
        boundaryJitterDoesNotDuplicate();
        std::cout << "All host clock regression cases passed\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
