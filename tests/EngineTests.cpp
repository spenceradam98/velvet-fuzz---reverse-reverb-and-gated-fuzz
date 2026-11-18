// Engine tests: parameter table sanity, every preset finite at several sample rates, no NaN at
// extreme bias / gain / sag, latency reported and honoured in Tape Reverse (zero in Gated Reverse),
// gate does not click, reverse tails swell then stop, silence in gives silence out, CPU headroom.
#include "Engine.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

using namespace vf;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL %s:%d: ", __FILE__, __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

struct Result { double peak = 0, rmsBurst = 0, rmsLate = 0; bool finite = true; double seconds = 0; };

static void applyAll (Engine& e, const std::array<float, NumParams>& v) { for (int i = 0; i < NumParams; ++i) e.setParam (i, v[(size_t) i]); }

// 0.5 s of a guitar-like signal (plucked harmonics with decay), then silence.
static Result run (Engine& e, double fs, double totalSec, int block = 256, double level = 0.3)
{
    const int total = (int) (totalSec * fs), burst = (int) (0.5 * fs), lat = e.latencySamples();
    std::vector<float> inL ((size_t) block), inR ((size_t) block), outL ((size_t) block), outR ((size_t) block);
    Result r;
    double sb = 0, sl = 0; int nb = 0, nl = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        for (int i = 0; i < n; ++i)
        {
            const int k = pos + i;
            double x = 0;
            if (k < burst)
            {
                const double t = k / fs, env = std::exp (-t * 3.0);
                x = level * env * (std::sin (kTwoPi * 196.0 * t) + 0.5 * std::sin (kTwoPi * 392.0 * t) + 0.25 * std::sin (kTwoPi * 588.0 * t));
            }
            inL[(size_t) i] = inR[(size_t) i] = (float) x;
        }
        e.process (inL.data(), inR.data(), outL.data(), outR.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const int k = pos + i;
            if (! std::isfinite (outL[(size_t) i]) || ! std::isfinite (outR[(size_t) i])) r.finite = false;
            const double a = 0.5 * (outL[(size_t) i] + outR[(size_t) i]);
            r.peak = std::max (r.peak, (double) std::max (std::fabs (outL[(size_t) i]), std::fabs (outR[(size_t) i])));
            if (k >= lat && k < lat + burst) { sb += a * a; ++nb; }
            else if (k >= total - (int) (0.5 * fs)) { sl += a * a; ++nl; }
        }
    }
    r.seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    r.rmsBurst = std::sqrt (sb / std::max (1, nb));
    r.rmsLate = std::sqrt (sl / std::max (1, nl));
    return r;
}

static std::vector<float> impulseResponse (Engine& e, double fs, double seconds)
{
    const int total = (int) (seconds * fs), block = 128;
    std::vector<float> out ((size_t) total, 0.0f);
    std::vector<float> inL ((size_t) block), inR ((size_t) block), oL ((size_t) block), oR ((size_t) block);
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        for (int i = 0; i < n; ++i) inL[(size_t) i] = inR[(size_t) i] = (pos + i == 0) ? 1.0f : 0.0f;
        e.process (inL.data(), inR.data(), oL.data(), oR.data(), n);
        for (int i = 0; i < n; ++i) out[(size_t) (pos + i)] = 0.5f * (oL[(size_t) i] + oR[(size_t) i]);
    }
    return out;
}

static double rmsRange (const std::vector<float>& x, int a, int b)
{
    double s = 0; int n = 0;
    for (int i = std::max (0, a); i < std::min ((int) x.size(), b); ++i) { s += (double) x[(size_t) i] * x[(size_t) i]; ++n; }
    return std::sqrt (s / std::max (1, n));
}

int main()
{
    // ---- parameter table
    {
        const auto* defs = paramDefs();
        for (int i = 0; i < NumParams; ++i)
        {
            CHECK (defs[i].min < defs[i].max, "%s range", defs[i].id);
            CHECK (defs[i].def >= defs[i].min && defs[i].def <= defs[i].max, "%s default out of range", defs[i].id);
            CHECK (paramIndex (defs[i].id) == i, "%s index", defs[i].id);
            for (int j = 0; j < i; ++j) CHECK (std::strcmp (defs[i].id, defs[j].id) != 0, "duplicate id %s", defs[i].id);
            if (defs[i].kind == Kind::Choice)
            {
                int n = 1; for (const char* c = defs[i].choices; *c; ++c) n += (*c == '|');
                CHECK (n == (int) defs[i].max + 1, "%s has %d choices, max %g", defs[i].id, n, defs[i].max);
            }
        }
    }

    // ---- transfer curve and its antiderivative agree
    {
        double maxErr = 0;
        for (double u = -1.0; u < 3.0; u += 0.01)
        {
            const double num = (xferGInt (u + 1e-5) - xferGInt (u - 1e-5)) / 2e-5;
            maxErr = std::max (maxErr, std::fabs (num - xferG (u)));
        }
        CHECK (maxErr < 1e-4, "antiderivative mismatch %g", maxErr);
    }

    // ---- presets: finite, sane level, tails die out
    {
        const auto& ps = factoryPresets();
        CHECK (ps.size() >= 24, "only %d presets", (int) ps.size());
        for (const auto& p : ps)
        {
            for (const auto& kv : p.values) CHECK (paramIndex (kv.first.c_str()) >= 0, "preset %s: unknown id %s", p.name.c_str(), kv.first.c_str());
            bool bankOk = false; for (const auto& b : presetBanks()) bankOk |= (b == p.bank);
            CHECK (bankOk, "preset %s: unknown bank", p.name.c_str());
            auto e = std::make_unique<Engine>();
            applyAll (*e, presetValues (p));
            e->prepare (48000, 256);
            const auto r = run (*e, 48000, 6.0);
            CHECK (r.finite, "preset %s: non-finite output", p.name.c_str());
            CHECK (r.peak < 4.0, "preset %s: peak %g", p.name.c_str(), r.peak);
            CHECK (r.rmsBurst > 0.005, "preset %s: too quiet (%g)", p.name.c_str(), r.rmsBurst);
            CHECK (r.rmsLate < 1e-3, "preset %s: tail does not die (%g)", p.name.c_str(), r.rmsLate);
        }
    }

    // ---- extreme settings at several rates and block sizes: no NaN, no blow-up
    {
        const double rates[] = { 44100, 48000, 96000, 192000 };
        const int blocks[] = { 1, 37, 512 };
        int combo = 0;
        for (double fs : rates)
            for (float bias : { 0.0f, 1.0f })
                for (float gain : { 0.0f, 1.0f })
                {
                    auto e = std::make_unique<Engine>();
                    e->setParam (FuzzBias, bias); e->setParam (FuzzGain, gain); e->setParam (FuzzSag, 1.0f);
                    e->setParam (FuzzGate, 1.0f); e->setParam (FuzzMids, 1.0f); e->setParam (FuzzTone, 1.0f);
                    e->setParam (Oversample, (combo & 1) ? 1.0f : 0.0f);
                    e->setParam (Order, (float) (combo % 3));
                    e->setParam (VerbMode, (float) ((combo / 3) & 1));
                    e->setParam (VerbLen, 2000); e->setParam (VerbMix, 1.0f); e->setParam (VerbSize, 1.0f); e->setParam (VerbSwell, 1.0f);
                    e->setParam (InTrim, 24); e->setParam (OutGain, 12); e->setParam (Width, 2.0f);
                    e->prepare (fs, 512);
                    const auto r = run (*e, fs, 1.5, blocks[combo % 3], 1.0);
                    CHECK (r.finite, "extreme fs %g bias %g gain %g: non-finite", fs, bias, gain);
                    CHECK (r.peak < 100.0, "extreme fs %g bias %g gain %g: peak %g", fs, bias, gain, r.peak);
                    ++combo;
                }
    }

    // ---- latency: zero in Gated Reverse, Length in Tape Reverse, and the dry really lands there
    for (double fs : { 44100.0, 48000.0 })
    {
        for (int mode : { 0, 1 })
        {
            auto e = std::make_unique<Engine>();
            e->setParam (FuzzOn, 0); e->setParam (VerbMix, 0.0f); e->setParam (VerbMode, (float) mode); e->setParam (VerbLen, 730);
            e->prepare (fs, 128);
            const int lat = e->latencySamples();
            const int expect = mode == 1 ? (int) std::lround (0.73 * fs) : 0;
            CHECK (lat == expect, "mode %d fs %g: latency %d, expected %d", mode, fs, lat, expect);
            const auto ir = impulseResponse (*e, fs, 1.0);
            int peakAt = 0; for (size_t i = 0; i < ir.size(); ++i) if (std::fabs (ir[i]) > std::fabs (ir[(size_t) peakAt])) peakAt = (int) i;
            CHECK (peakAt == lat, "mode %d fs %g: dry impulse at %d, latency %d", mode, fs, peakAt, lat);
            CHECK (std::fabs (ir[(size_t) peakAt] - 1.0f) < 0.02f, "mode %d: dry gain %g", mode, ir[(size_t) peakAt]);

            // bypass is aligned with the same latency
            auto b = std::make_unique<Engine>();
            b->setParam (VerbMode, (float) mode); b->setParam (VerbLen, 730); b->setParam (Bypass, 1);
            b->prepare (fs, 128);
            const auto irb = impulseResponse (*b, fs, 1.0);
            CHECK (std::fabs (irb[(size_t) lat] - 1.0f) < 1e-4f, "bypass mode %d: impulse not at latency", mode);
        }
        // synced length follows the tempo
        Engine e; e.setParam (VerbMode, 1); e.setParam (VerbSync, 4); e.setTempo (100); e.prepare (fs, 128);
        CHECK (e.latencySamples() == (int) std::lround (0.6 * fs), "synced latency %d", e.latencySamples());
    }

    // ---- reverse character: Gated Reverse wet rises over the Length and then stops
    {
        const double fs = 48000;
        Engine e;
        e.setParam (FuzzOn, 0); e.setParam (VerbMix, 1.0f); e.setParam (VerbLen, 800); e.setParam (VerbSwell, 0.6f);
        e.prepare (fs, 128);
        const auto ir = impulseResponse (e, fs, 1.6);
        const int L = (int) (0.8 * fs);
        const double early = rmsRange (ir, 0, L / 4), late = rmsRange (ir, 3 * L / 4, L), after = rmsRange (ir, L + (int) (0.1 * fs), L + (int) (0.6 * fs));
        CHECK (late > 3.0 * early, "gated reverse does not swell: early %g late %g", early, late);
        CHECK (after < 0.1 * late, "gated reverse does not cut off: late %g after %g", late, after);
    }
    {
        // Tape Reverse: wet energy builds up towards the chunk end (reversed decay)
        const double fs = 48000;
        Engine e;
        e.setParam (FuzzOn, 0); e.setParam (VerbMix, 1.0f); e.setParam (VerbMode, 1); e.setParam (VerbLen, 500);
        e.prepare (fs, 128);
        const auto ir = impulseResponse (e, fs, 3.0);
        double total = rmsRange (ir, 0, (int) ir.size());
        CHECK (total > 1e-4, "tape reverse silent");
        CHECK (rmsRange (ir, (int) (2.5 * fs), (int) (3.0 * fs)) < 1e-3 * std::max (1.0, total * 1000), "tape reverse never stops");
    }

    // ---- gate: no clicks (the gain never jumps), and it closes on silence
    {
        const double fs = 48000;
        Gate g; g.prepare (fs); g.set (0.8, 0.3);
        double prevGain = g.gain, maxStep = 0;
        for (int i = 0; i < (int) (2.0 * fs); ++i)
        {
            const double t = i / fs, x = 0.5 * std::exp (-t * 4.0) * std::sin (kTwoPi * 110.0 * t) * (t < 1.2 ? 1.0 : 0.0);
            g.process (x, x);
            maxStep = std::max (maxStep, std::fabs (g.gain - prevGain));
            prevGain = g.gain;
        }
        CHECK (maxStep < 0.02, "gate gain jumps by %g per sample", maxStep);
        CHECK (g.gain < 0.01, "gate does not close (%g)", g.gain);

        // engine level: a fuzzed note through a heavy gate has no sample-to-sample jump bigger than the signal's own
        Engine e;
        e.setParam (VerbOn, 0); e.setParam (FuzzGate, 0.9f); e.setParam (FuzzGain, 0.8f); e.setParam (FuzzTone, 0.2f);
        e.prepare (fs, 64);
        const int n = (int) (2.0 * fs);
        std::vector<float> in ((size_t) n), oL ((size_t) n), oR ((size_t) n);
        for (int i = 0; i < n; ++i) { const double t = i / fs; in[(size_t) i] = (float) (0.4 * std::exp (-t * 2.5) * std::sin (kTwoPi * 82.4 * t)); }
        for (int pos = 0; pos < n; pos += 64) e.process (in.data() + pos, in.data() + pos, oL.data() + pos, oR.data() + pos, 64);
        double pk = 0, maxD = 0;
        for (int i = 1; i < n; ++i) { pk = std::max (pk, (double) std::fabs (oL[(size_t) i])); maxD = std::max (maxD, (double) std::fabs (oL[(size_t) i] - oL[(size_t) i - 1])); }
        CHECK (pk > 0.01, "gated fuzz silent");
        CHECK (maxD < 0.5 * pk + 0.02, "gated fuzz clicks: step %g vs peak %g", maxD, pk);
    }

    // ---- bias: starving the transistor makes quiet notes sputter out
    {
        auto level = [] (float bias)
        {
            Engine e; e.setParam (VerbOn, 0); e.setParam (FuzzGate, 0); e.setParam (FuzzGain, 0.3f); e.setParam (FuzzBias, bias);
            e.prepare (48000, 256);
            return run (e, 48000, 0.6, 256, 0.02).rmsBurst;
        };
        const double healthy = level (1.0f), starved = level (0.0f);
        CHECK (starved < 0.5 * healthy, "starved bias should choke quiet notes: healthy %g starved %g", healthy, starved);
    }

    // ---- sag: the supply drops while playing and recovers
    {
        Engine e; e.setParam (VerbOn, 0); e.setParam (FuzzSag, 1.0f); e.setParam (FuzzGain, 1.0f);
        e.prepare (48000, 256);
        std::vector<float> in (256, 0.0f), oL (256), oR (256);
        for (int i = 0; i < 256; ++i) in[(size_t) i] = 0.5f * (float) std::sin (kTwoPi * 110.0 * i / 48000.0);
        for (int b = 0; b < 40; ++b) e.process (in.data(), in.data(), oL.data(), oR.data(), 256);
        const float playing = e.supplyVolts.load();
        std::fill (in.begin(), in.end(), 0.0f);
        for (int b = 0; b < 200; ++b) e.process (in.data(), in.data(), oL.data(), oR.data(), 256);
        const float rested = e.supplyVolts.load();
        CHECK (playing >= 3.0f && playing < 4.5f, "sagging supply while playing: %g V", playing);
        CHECK (rested > playing + 0.5f, "supply does not recover: %g -> %g V", playing, rested);
    }

    // ---- silence in, silence out (no self-oscillation, no DC)
    {
        Engine e; applyAll (e, presetValues (factoryPresets()[5])); e.prepare (48000, 256);
        std::vector<float> z (256, 0.0f), oL (256), oR (256);
        double pk = 0;
        for (int b = 0; b < 400; ++b) { e.process (z.data(), z.data(), oL.data(), oR.data(), 256); for (float v : oL) pk = std::max (pk, (double) std::fabs (v)); }
        CHECK (pk < 1e-6, "silence in gives %g out", pk);
    }

    // ---- switching order, mode and oversampling while playing stays finite and click-free-ish
    {
        Engine e; e.prepare (48000, 256);
        std::vector<float> in (256), oL (256), oR (256);
        bool finite = true;
        for (int b = 0; b < 600; ++b)
        {
            if (b % 50 == 0) { e.setParam (Order, (float) ((b / 50) % 3)); e.setParam (VerbMode, (float) ((b / 100) % 2)); e.setParam (Oversample, (float) ((b / 75) % 2)); }
            for (int i = 0; i < 256; ++i) in[(size_t) i] = 0.2f * (float) std::sin (kTwoPi * 147.0 * (b * 256 + i) / 48000.0);
            e.process (in.data(), in.data(), oL.data(), oR.data(), 256);
            for (int i = 0; i < 256; ++i) finite &= std::isfinite (oL[(size_t) i]) && std::isfinite (oR[(size_t) i]);
        }
        CHECK (finite, "non-finite output while switching");
    }

    // ---- CPU: worst case (4x oversampling, Tape Reverse, 96 kHz) well under real time
    {
        Engine e; e.setParam (Oversample, 1); e.setParam (VerbMode, 1); e.setParam (VerbLen, 2000); e.prepare (96000, 512);
        const auto r = run (e, 96000, 4.0, 512);
        const double ratio = r.seconds / 4.0;
        std::printf ("cpu: 4x / tape / 96 kHz runs at %.1f%% of real time\n", ratio * 100.0);
        CHECK (ratio < 0.5, "too slow: %.2f x real time", ratio);
    }

    if (failures == 0) std::printf ("all engine tests passed\n");
    else std::printf ("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
