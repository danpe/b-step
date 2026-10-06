# Continuous host tempo

The plugin clock follows host quarter-note position instead of multiplying
absolute sample time by the newest tempo. It interpolates within the current
block at 24 clocks per quarter, retains the speed divisor and one-clock
precalculation lead, and recognizes intentional seeks, loop wraps, stop/resume
and speed changes. Signed preroll is handled without unsigned wraparound.
Clocks do not duplicate when a block boundary rounds backward by one sample.
There is no allocation, host API call or blocking lock in the new clock.

## Required REAPER setup for live controls

Disable **anticipative FX processing** on each track containing B-Step. The
matched sequencer-player patch applies `I_PERFFLAGS | 2` to those tracks at
startup and preserves other track flags. It leaves the audio device, sample
rate, hardware buffer size and other tracks alone. Save the project to retain
these flags without the player running.

This matters independently of the plugin's clock calculation. On REAPER 7.80,
changing global BPM while anticipative processing is enabled can rebase both
sample and musical position in already-rendered blocks. Rapid edits can reach
different tracks at different rendered positions. Accumulating a separate
phase correction per plugin would allow those tracks to drift apart.

With anticipative processing disabled on the seven B-Step tracks, the captured
host musical position is continuous across global tempo edits, even while
sample position rebases. All seven real instances produced identical clocks
and MIDI output through the isolated host test. The clock follows that shared
host position directly, without a compensation heuristic or extra tempo markers.
Moving these tracks to real-time processing increases the real-time audio-thread
work, so check underruns and CPU load with the actual project before deployment.

## Regression tests

```sh
cmake -S . -B clock-tests -DBSTEP_CLOCK_TESTS_ONLY=ON
cmake --build clock-tests
ctest --test-dir clock-tests --output-on-failure
```

Recent Apple toolchains may need `-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0`.
Cases cover constant and fractional tempo, sample rates, buffer sizes, speed,
six-hour timelines, tempo-map history, seeks, loops, stop/resume, invalid input,
negative preroll and block-boundary rounding.

`tests/fixtures/reaper-7.80-realtime-tempo.csv` records the first playback
segment of a seven-instance VST3 test on REAPER 7.80, 48 kHz / 1024 samples,
starting at 120 seconds. It includes 47 tempo edits (including a 10 Hz sweep),
with 715 consecutive clocks and no tempo-triggered seek or stop callback.
The earlier `reaper-7.80-global-tempo.csv` capture retains evidence of the
render-ahead discontinuity; it is not a claim of continuity in that configuration.

Optional test instrumentation uses `BSTEP_HOST_CLOCK_TRACE=ON` and
`BSTEP_CLOCK_TRACE_DIR` pointing at an existing empty directory. A bounded
single-producer ring feeds a message-thread file writer; overflow is reported.
Events 0, 1-5, 10 and 20 mean block, transport, clock and MIDI note respectively.
Production builds leave tracing OFF. Plugin IDs, parameter order, stored state,
standalone/external MIDI clock and the fork's custom MIDI behavior are unchanged.

## Matched deployment and rollback

Deploy the plugin, player change and B-Step track processing flags together.
The player retains deliberate idle/resume but removes the BPM-triggered Stop/Play
workaround. Back up the live and saved project, old VST3 bundle and deployed
player first. For rollback restore both code components and the prior track
processing flags, retaining later musical edits and the independent Teensy
recovery changes.
