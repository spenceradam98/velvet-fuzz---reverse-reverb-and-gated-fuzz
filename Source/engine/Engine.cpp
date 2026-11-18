#include "Engine.h"
#include <cstring>

namespace vf
{
int paramIndex (const char* id)
{
    const auto* defs = paramDefs();
    for (int i = 0; i < NumParams; ++i)
        if (std::strcmp (defs[i].id, id) == 0) return i;
    return -1;
}

Engine::Engine()
{
    const auto* defs = paramDefs();
    for (int i = 0; i < NumParams; ++i) values[(size_t) i] = defs[i].def;
}

double Engine::lengthMs() const
{
    const double beats = syncBeats ((int) values[VerbSync]);
    const double ms = beats > 0 ? beats * 60000.0 / tempo : (double) values[VerbLen];
    return clampd (ms, 100.0, kMaxLenMs);
}

static int tapeLengthSamples (double ms, double fs)
{
    // quantised to 10 ms so small knob moves don't keep re-cutting the tape (and moving the latency)
    const double q = std::round (ms / 10.0) * 10.0;
    return (int) std::lround (q * 0.001 * fs);
}

int Engine::latencySamples() const
{
    return (int) values[VerbMode] == 1 ? tapeLengthSamples (lengthMs(), fs) : 0;
}

void Engine::prepare (double sampleRate, int)
{
    fs = sampleRate;
    gated.prepare (fs, kMaxLenMs + 20);
    tape.prepare (fs, kMaxLenMs + 20);
    activeOs = (int) values[Oversample] == 1 ? 4 : 2;
    fuzz.prepare (fs, activeOs);
    const int maxLen = (int) ((kMaxLenMs + 20) * 0.001 * fs) + 8;
    preL.allocate ((int) (0.26 * fs) + 8); preR.allocate ((int) (0.26 * fs) + 8);
    alignL.allocate (maxLen); alignR.allocate (maxLen);
    bypL.allocate (maxLen); bypR.allocate (maxLen);

    transition.prepare (fs, 0.004, 1.0);
    verbOn.prepare (fs, 0.02, values[VerbOn] > 0.5f ? 1.0 : 0.0);
    fuzzOn.prepare (fs, 0.01, values[FuzzOn] > 0.5f ? 1.0 : 0.0);
    mix.prepare (fs, 0.03, values[VerbMix]);
    inTrim.prepare (fs, 0.03, dbToGain (values[InTrim]));
    outGain.prepare (fs, 0.03, dbToGain (values[OutGain]));
    fuzzLevel.prepare (fs, 0.03, dbToGain (values[FuzzLevel]));
    width.prepare (fs, 0.05, values[Width]);
    bypass.prepare (fs, 0.015, values[Bypass] > 0.5f ? 1.0 : 0.0);
    preDelay.prepare (fs, 0.1, values[VerbPre] * 0.001 * fs);

    activeOrder = (int) values[Order];
    activeMode = (int) values[VerbMode];
    activeTapeLen = tapeLengthSamples (lengthMs(), fs);
    tape.setLength (activeTapeLen);
    update();
    reset();
}

void Engine::reset()
{
    gated.clear(); tape.clear(); fuzz.reset(); fuzz.snapControls();
    preL.clear(); preR.clear(); alignL.clear(); alignR.clear(); bypL.clear(); bypR.clear();
    transition.snap(); verbOn.snap(); fuzzOn.snap(); mix.snap(); inTrim.snap(); outGain.snap();
    fuzzLevel.snap(); width.snap(); bypass.snap(); preDelay.snap();
    meterIn = meterOut = meterWet = 0;
}

void Engine::update()
{
    const double lenSec = lengthMs() * 0.001;
    gated.configure (lenSec, values[VerbSwell], values[VerbSize], values[VerbDamp]);
    tape.configure (values[VerbSwell], values[VerbSize], values[VerbDamp]);
    fuzz.configure (values[FuzzGain], values[FuzzBias], values[FuzzSag], values[FuzzGate], values[FuzzTone], values[FuzzMids]);

    verbOn.set (values[VerbOn] > 0.5f ? 1.0 : 0.0);
    fuzzOn.set (values[FuzzOn] > 0.5f ? 1.0 : 0.0);
    mix.set (values[VerbMix]);
    inTrim.set (dbToGain (values[InTrim]));
    outGain.set (dbToGain (values[OutGain]));
    fuzzLevel.set (dbToGain (values[FuzzLevel]));
    width.set (values[Width]);
    bypass.set (values[Bypass] > 0.5f ? 1.0 : 0.0);
    preDelay.set (clampd (values[VerbPre] * 0.001 * fs, 0.0, (double) preL.capacity() - 4));
}

inline void Engine::verbSection (double inL, double inR, double& dryL, double& dryR, double& wetL, double& wetR)
{
    // dry, aligned with the Tape Reverse latency
    alignL.push (inL); alignR.push (inR);
    const int d = activeMode == 1 ? activeTapeLen : 0;
    dryL = alignL.tap (d); dryR = alignR.tap (d);

    const double von = verbOn.next();
    if (von < 1e-5 && verbOn.target < 0.5) { wetL = wetR = 0; return; }

    preL.push (inL); preR.push (inR);
    const double pd = preDelay.next();
    const double xl = pd < 0.5 ? inL : preL.tapLin (pd), xr = pd < 0.5 ? inR : preR.tapLin (pd);
    if (activeMode == 1) tape.process (xl, xr, wetL, wetR);
    else gated.process (xl, xr, wetL, wetR);
    wetL *= von; wetR *= von;
}

void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    update();

    const int wantOrder = (int) values[Order];
    const int wantMode = (int) values[VerbMode];
    const int wantLen = tapeLengthSamples (lengthMs(), fs);
    const int wantOs = (int) values[Oversample] == 1 ? 4 : 2;
    const bool pending = wantOrder != activeOrder || wantMode != activeMode || wantOs != activeOs
                         || (wantMode == 1 && wantLen != activeTapeLen);
    transition.set (pending ? 0.0 : 1.0);

    double pkIn = 0, pkOut = 0, pkWet = 0, voltsMin = 9.0;
    for (int i = 0; i < n; ++i)
    {
        // apply a pending structural change once the output has faded out
        if (pending && transition.v < 1e-3)
        {
            if (wantMode != activeMode) { gated.clear(); tape.clear(); }
            if (wantMode == 1 && wantLen != activeTapeLen) { tape.clear(); }
            if (wantOs != activeOs) { fuzz.setOversampling (wantOs); activeOs = wantOs; }
            activeOrder = wantOrder; activeMode = wantMode; activeTapeLen = wantLen;
            tape.setLength (activeTapeLen);
            transition.set (1.0);
        }

        const double rawL = inL[i], rawR = inR[i];
        bypL.push (rawL); bypR.push (rawR);

        const double trim = inTrim.next();
        const double xl = rawL * trim, xr = rawR * trim;
        pkIn = std::max (pkIn, std::max (std::fabs (xl), std::fabs (xr)));

        const double m = mix.next();
        const double dg = std::min (1.0, 2.0 * (1.0 - m)), wg = std::min (1.0, 2.0 * m);
        const double fon = fuzzOn.next(), flev = fuzzLevel.next();

        auto runFuzz = [&] (double& l, double& r)
        {
            if (fon < 1e-5 && fuzzOn.target < 0.5) return;
            double fl = l, fr = r;
            fuzz.process (fl, fr);
            l = lerp (l, fl * flev, fon);
            r = lerp (r, fr * flev, fon);
            voltsMin = std::min (voltsMin, fuzz.volts);
        };

        double yl, yr, dl, dr, wl, wr;
        switch (activeOrder)
        {
            case 1:   // Fuzz > Verb
            {
                double fl = xl, fr = xr;
                runFuzz (fl, fr);
                verbSection (fl, fr, dl, dr, wl, wr);
                const double von = verbOn.v, dgo = lerp (1.0, dg, von);
                yl = dl * dgo + wl * wg; yr = dr * dgo + wr * wg;
                break;
            }
            case 2:   // Parallel: fuzz of the (aligned) dry, plus the reverse wet of the clean input
            {
                verbSection (xl, xr, dl, dr, wl, wr);
                runFuzz (dl, dr);
                const double von = verbOn.v, dgo = lerp (1.0, dg, von);
                yl = dl * dgo + wl * wg; yr = dr * dgo + wr * wg;
                break;
            }
            default:  // Verb > Fuzz
            {
                verbSection (xl, xr, dl, dr, wl, wr);
                const double von = verbOn.v, dgo = lerp (1.0, dg, von);
                yl = dl * dgo + wl * wg; yr = dr * dgo + wr * wg;
                runFuzz (yl, yr);
                break;
            }
        }
        pkWet = std::max (pkWet, std::max (std::fabs (wl), std::fabs (wr)));

        // width (mid/side), output, transition mute
        const double w = width.next();
        const double mid = 0.5 * (yl + yr), side = 0.5 * (yl - yr) * w;
        const double g = outGain.next() * transition.next();
        yl = (mid + side) * g; yr = (mid - side) * g;

        // bypass crossfades to the input, delayed by the same latency the host compensates
        const double b = bypass.next();
        if (b > 1e-6)
        {
            const int d = activeMode == 1 ? activeTapeLen : 0;
            yl = lerp (yl, bypL.tap (d), b);
            yr = lerp (yr, bypR.tap (d), b);
        }
        if (isBad (yl) || isBad (yr)) { yl = yr = 0; }   // never pass NaN / inf to the host
        outL[i] = (float) yl;
        outR[i] = (float) yr;
        pkOut = std::max (pkOut, std::max (std::fabs (yl), std::fabs (yr)));
    }

    const double decay = std::exp (-(double) n / (0.25 * fs));
    meterIn = (float) std::max (pkIn, meterIn * decay);
    meterOut = (float) std::max (pkOut, meterOut * decay);
    meterWet = (float) std::max (pkWet, meterWet * decay);
    inLevel.store (meterIn);
    outLevel.store (meterOut);
    wetLevel.store (meterWet);
    supplyVolts.store (values[FuzzOn] > 0.5f ? (float) voltsMin : 9.0f);
    gateLevel.store ((float) fuzz.gateGain());
}

// ---------------------------------------------------------------- presets
std::array<float, NumParams> presetValues (const Preset& p)
{
    std::array<float, NumParams> v {};
    const auto* defs = paramDefs();
    for (int i = 0; i < NumParams; ++i) v[(size_t) i] = defs[i].def;
    for (const auto& kv : p.values)
    {
        const int idx = paramIndex (kv.first.c_str());
        if (idx >= 0) v[(size_t) idx] = clampd (kv.second, defs[idx].min, defs[idx].max);
    }
    return v;
}
} // namespace vf
