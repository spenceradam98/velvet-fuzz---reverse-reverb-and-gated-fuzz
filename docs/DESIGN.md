# Velvet Fuzz: design

The shoegaze glide-guitar chain in one box: a reverse reverb and a starved, gated fuzz.
JUCE 8 + CMake, VST3 + AU + Standalone, macOS 11+ (universal, Intel safe: scalar C++, no `-march`) and Windows (static CRT).
Family: psychedelic & shoegaze, next to driftglass, penumbra and emerald-haze.

## Signal flow

```
Verb > Fuzz :  in -> trim -> [REVERSE: dry + reversed wet] -> [FUZZ] -> width -> out
Fuzz > Verb :  in -> trim -> [FUZZ] -> [REVERSE: dry + reversed wet] -> width -> out
Parallel    :  in -> trim -+-> [FUZZ of the dry] --------+-> width -> out
                           +-> [REVERSE wet of the dry] -+
```

Reverse mix law: dry = min(1, 2 (1 - mix)), wet = min(1, 2 mix), so 50% is both at full level and 100% is wet only.
Changing Order, Reverse Mode, Oversampling or the Tape Reverse Length fades the output out for a few
milliseconds, applies the change, and fades back in, so switching never clicks.

## Controls

| Section | Controls |
|---|---|
| Routing | Order: Verb > Fuzz, Fuzz > Verb, Parallel (combo, or click the patch cable) |
| Reverse | On (footswitch), Mode (Gated Reverse / Tape Reverse), Length 100 ms to 2 s or tempo synced (1/16 to 1/1), Swell, Size, Damping, Pre-delay, Mix |
| Fuzz | On (footswitch), Gain, Bias, Sag, Gate, Tone, Mids (scoop / flat / push), Level |
| Global | Input trim, Width, Output, Oversampling (2x default / 4x), Bypass |

## DSP

- **Gated Reverse (zero latency).** Input through four allpass diffusers (Size sets their length and density),
  damped, into a delay line read by 96 jittered taps spread over the Length (different taps per side). Tap gain
  is `t^k` with `k` set by Swell, normalised for constant energy, and it stops dead at the Length: the 80s rack
  "reverse" program. Length changes glide (the taps move smoothly).
- **Tape Reverse (latency = Length).** An 8-line FDN hall (the driftglass one, trimmed) whose RT60 is tied to the
  Length (Swell shortens it, so the reversed swell gets steeper), written into a buffer and read backwards by two
  heads half a chunk apart under sin² windows (which sum to one). The plugin reports Length as latency and the
  engine delays the dry (and the fuzz in Parallel) by the same amount, so the host keeps everything aligned.
  Length is quantised to 10 ms in this mode so small knob moves don't keep moving the latency.
- **Fuzz.** Two transistor stages with a smoothstep transfer curve: zero below cutoff, flat at saturation. Bias
  moves stage one's operating point toward cutoff, so small signals vanish and decays sputter (the "velcro"
  sound). First-order ADAA on both stages (closed-form antiderivative) plus 2x / 4x oversampling with a
  12th-order Butterworth up/down filter (no reported latency). An interstage coupling cap, then Tone (low-pass
  700 Hz to 12 kHz) and Mids (750 Hz peak, -14 dB scoop to +8 dB push) at the host rate.
- **Sag.** The supply voltage drops from 9 V toward 3 V with the Sag knob and with an envelope follower on the
  output current. Lower voltage shrinks the headroom and starves the bias further: squishy attack, ragged decay.
  The voltmeter on the pedal shows the live rail.
- **Gate.** A 1:3 downward expander after the fuzz, keyed by the fuzz input so it follows the playing rather than
  the clipped output. Its threshold rises with Gate and also as Bias goes down, so starved settings choke
  naturally. Opening 1.5 ms, closing 30 ms, smoothed per sample (tested not to click).
- **Bypass** crossfades to the input delayed by the current latency, so it stays aligned in Tape Reverse.

## UI

A pedalboard: two stompboxes that swap sides when the order flips, joined by a patch cable you click to step
through the orders (Parallel shows a small blend box). Footswitches switch each pedal; the LEDs follow.
The reverse pedal shows its swell shape and the current latency; the fuzz pedal has a supply voltmeter and a
gate lamp. Six themes: Pink Bleach (new, the default), and Dusk, Neon, Fuzz, Velvet, Mono from driftglass.
Theme and window size are saved with the session; the window scales from 70% to 200%.

## Presets

25 factory presets in five banks: Glide, Wall, Reverse, Fuzz, Parallel & Utility (Glide Guitar, Loveless Wall,
Backwards Halo, Dying Battery, Velcro Splat, Starved Bass, Sitar Swell, Synth Smear, Reverse Pad Only,
Parallel Haze and more). User presets show up as a sixth bank. Presets never change the Bypass state.

## Tests (`tests/EngineTests.cpp`)

Parameter table sanity; transfer curve matches its antiderivative; every preset finite, audible and with a tail
that dies away; no NaN or blow-up at extreme bias / gain / sag / trim at 44.1 to 192 kHz and odd block sizes;
latency is 0 in Gated Reverse and exactly the Length in Tape Reverse (also tempo synced), and the dry impulse and
the bypass land exactly there; Gated Reverse swells then cuts off; gate gain never jumps and the gated fuzz does
not click; a starved bias chokes quiet notes; the sag rail drops while playing and recovers; silence in gives
silence out; switching order / mode / oversampling while playing stays finite; worst-case CPU (4x, Tape, 96 kHz)
runs at about 7% of real time on the build machine.

## Decisions taken for v0.1

- Name: Velvet Fuzz (`PRODUCT_NAME` in CMakeLists.txt). Plugin code `Vfz1`, manufacturer `Vlvf`.
- Gated Reverse is the default mode (zero latency, safe live); Tape Reverse is labelled with its latency on the pedal.
- 2x oversampling is the default (old Intel Macs); 4x is one click away.
- Tape Reverse is the classic reverse-delay chunking of a reverb tail, not a true time-reversed convolution, so
  the swell lands on average (not exactly) at the note. Reporting Length as latency centres it.
- Oversampling uses IIR filters (no latency) rather than linear-phase FIR; aliasing is handled by ADAA + the filters.

## Later

Polyphase half-band oversampling, a true reversed-convolution mode, an octave-up fuzz voice, MIDI-synced swell retrigger.
