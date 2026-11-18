// The two reverse reverbs. Both take mono-ish stereo in and give a wet-only stereo out.
//  - GatedReverse: zero latency. A diffused tapped delay whose tap gains rise over the Length, then stop
//    (the 80s rack "reverse" program). Feels backwards, needs no lookahead.
//  - TapeReverse: a diffuse FDN reverb written into a buffer and read backwards by two heads in
//    overlapping chunks with sin^2 windows. Latency = Length (the engine reports it and delays the dry).
#pragma once

#include "Dsp.h"
#include <array>

namespace vf
{
// ---------------------------------------------------------------- 8-line FDN (the driftglass hall, trimmed)
class Fdn
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int maxLine = (int) (kLenMs[N - 1] * 0.001 * fs * kMaxSize) + (int) (40 * fs / 48000.0) + 8;
        for (int i = 0; i < N; ++i)
        {
            lines[(size_t) i].allocate (maxLine);
            lfo[(size_t) i].setPhase (i * 0.137);
        }
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < kDiff; ++k)
                diff[(size_t) c][(size_t) k].allocate ((int) (kDiffMs[c][k] * 0.001 * fs * 2.2) + 4);
        configure (1.0, 2.0, 6000);
        clear();
    }

    void clear()
    {
        for (auto& l : lines) l.clear();
        for (auto& d : damp) d.reset();
        for (auto& ch : diff) for (auto& ap : ch) ap.clear();
    }

    // size 0.3..3 (delay scale), rt60 in seconds, damping corner in Hz
    void configure (double size, double rt60, double dampHz)
    {
        size = clampd (size, 0.3, kMaxSize);
        for (int i = 0; i < N; ++i)
        {
            len[(size_t) i] = kLenMs[i] * 0.001 * fs * size;
            gain[(size_t) i] = std::pow (10.0, -3.0 * (len[(size_t) i] / fs) / std::max (0.05, rt60));
            damp[(size_t) i].set (fs, dampHz * (1.0 - 0.03 * i));
            lfo[(size_t) i].set (fs, 0.5 * (0.75 + 0.09 * i));
        }
        modSamples = 4.0 * fs / 48000.0 * std::sqrt (size);
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < kDiff; ++k)
                diff[(size_t) ch][(size_t) k].len = std::max (2.0, kDiffMs[ch][k] * 0.001 * fs * (0.6 + 0.6 * std::min (size, 2.0)));
    }

    inline void process (double inL, double inR, double& outL, double& outR)
    {
        for (int k = 0; k < kDiff; ++k)
        {
            inL = diff[0][(size_t) k].process (inL, 0.62);
            inR = diff[1][(size_t) k].process (inR, 0.62);
        }
        std::array<double, N> o {};
        double sum = 0;
        for (int i = 0; i < N; ++i)
        {
            o[(size_t) i] = lines[(size_t) i].tapHermite (len[(size_t) i] + modSamples * (1.0 + lfo[(size_t) i].sine()));
            lfo[(size_t) i].tick();
            sum += o[(size_t) i];
        }
        const double h = sum * (2.0 / N);
        outL = outR = 0;
        for (int i = 0; i < N; ++i)
        {
            const double y = damp[(size_t) i].process (o[(size_t) i] - h) * gain[(size_t) i];
            lines[(size_t) i].push (y + ((i & 1) ? inR : inL) * 0.5);
            outL += o[(size_t) i] * kSignL[i];
            outR += o[(size_t) i] * kSignR[i];
        }
        outL *= 0.42; outR *= 0.42;
    }

private:
    static constexpr int N = 8, kDiff = 4;
    static constexpr double kMaxSize = 3.0;
    static constexpr double kLenMs[N] = { 31.3, 37.9, 41.7, 47.3, 53.1, 61.7, 67.9, 79.3 };
    static constexpr double kDiffMs[2][kDiff] = { { 4.77, 3.59, 12.73, 9.31 }, { 5.13, 3.27, 11.91, 8.93 } };
    static constexpr double kSignL[N] = { 1, -1, 1, 1, -1, 1, -1, 1 };
    static constexpr double kSignR[N] = { 1, 1, -1, 1, 1, -1, -1, -1 };

    double fs = 48000, modSamples = 0;
    std::array<DelayLine, N> lines;
    std::array<double, N> len {}, gain {};
    std::array<OnePoleLP, N> damp;
    std::array<Lfo, N> lfo;
    std::array<std::array<Allpass, kDiff>, 2> diff;
};

// Tap gain curve shared by both modes: how the swell rises over the Length.
// swell 0 = early, gentle rise (almost flat); swell 1 = everything piles up at the very end.
inline double swellExponent (double swell) { return 0.4 + 5.0 * clampd (swell, 0, 1) * clampd (swell, 0, 1) + 1.2 * clampd (swell, 0, 1); }

// ---------------------------------------------------------------- Gated Reverse
class GatedReverse
{
public:
    static constexpr int kTaps = 96;

    void prepare (double sampleRate, double maxLenMs)
    {
        fs = sampleRate;
        for (int c = 0; c < 2; ++c)
        {
            line[(size_t) c].allocate ((int) (maxLenMs * 0.001 * fs) + 16);
            for (int k = 0; k < kDiff; ++k)
            {
                pre[(size_t) c][(size_t) k].allocate ((int) (kDiffMs[k] * 1.12 * 0.001 * fs * 1.7) + 8);
            }
            for (int k = 0; k < kPost; ++k) post[(size_t) c][(size_t) k].allocate ((int) (kPostMs[k] * 1.12 * 0.001 * fs) + 8);
        }
        // fixed, slightly jittered tap positions (fractions of Length), different per side
        Rng rng (0xC0FFEEu);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kTaps; ++i)
                frac[(size_t) c][(size_t) i] = clampd ((i + 0.5 + 0.8 * rng.bi() * 0.5) / kTaps, 0.002, 0.999);
        lenSm.prepare (fs, 0.08, 0.6 * fs);
        configure (0.6, 0.5, 0.6, 0.35);
        lenSm.snap();
        clear();
    }

    void clear()
    {
        for (auto& l : line) l.clear();
        for (auto& ch : pre) for (auto& ap : ch) ap.clear();
        for (auto& ch : post) for (auto& ap : ch) ap.clear();
        for (auto& d : damp) d.reset();
    }

    void configure (double lengthSec, double swell, double size, double damping)
    {
        lenSm.set (clampd (lengthSec * fs, 16.0, (double) line[0].capacity() - 4));
        if (std::fabs (swell - lastSwell) > 1e-4)
        {
            lastSwell = swell;
            const double ex = swellExponent (swell);
            for (int c = 0; c < 2; ++c)
            {
                double e = 0;
                for (int i = 0; i < kTaps; ++i)
                {
                    const double g = std::pow (frac[(size_t) c][(size_t) i], ex) * ((i & 1) ? -1.0 : 1.0);
                    tapGain[(size_t) c][(size_t) i] = g;
                    e += g * g;
                }
                const double norm = 0.55 / std::sqrt (std::max (1e-9, e));
                for (auto& g : tapGain[(size_t) c]) g *= norm;
            }
        }
        const double s = 0.5 + 1.1 * clampd (size, 0, 1);
        for (int c = 0; c < 2; ++c)
        {
            for (int k = 0; k < kDiff; ++k) pre[(size_t) c][(size_t) k].len = std::max (2.0, kDiffMs[k] * (c ? 1.07 : 1.0) * 0.001 * fs * s);
            for (int k = 0; k < kPost; ++k) post[(size_t) c][(size_t) k].len = std::max (2.0, kPostMs[k] * (c ? 1.11 : 1.0) * 0.001 * fs);
        }
        diffG = 0.5 + 0.22 * clampd (size, 0, 1);
        const double hz = 14000.0 * std::pow (1500.0 / 14000.0, clampd (damping, 0, 1));
        damp[0].set (fs, hz); damp[1].set (fs, hz);
    }

    inline void process (double inL, double inR, double& outL, double& outR)
    {
        const double len = lenSm.next();
        double in[2] = { inL, inR }, out[2] = { 0, 0 };
        for (int c = 0; c < 2; ++c)
        {
            double x = in[c];
            for (int k = 0; k < kDiff; ++k) x = pre[(size_t) c][(size_t) k].process (x, diffG);
            line[(size_t) c].push (damp[(size_t) c].process (x));
            double acc = 0;
            const auto& f = frac[(size_t) c];
            const auto& g = tapGain[(size_t) c];
            for (int i = 0; i < kTaps; ++i) acc += g[(size_t) i] * line[(size_t) c].tapLin (f[(size_t) i] * len);
            for (int k = 0; k < kPost; ++k) acc = post[(size_t) c][(size_t) k].process (acc, 0.55);
            out[c] = acc;
        }
        outL = out[0]; outR = out[1];
    }

private:
    static constexpr int kDiff = 4, kPost = 2;
    static constexpr double kDiffMs[kDiff] = { 3.1, 5.3, 8.9, 13.7 };
    static constexpr double kPostMs[kPost] = { 4.3, 7.1 };

    double fs = 48000, diffG = 0.6, lastSwell = -1;
    std::array<DelayLine, 2> line;
    std::array<std::array<Allpass, kDiff>, 2> pre;
    std::array<std::array<Allpass, kPost>, 2> post;
    std::array<OnePoleLP, 2> damp;
    std::array<std::array<double, kTaps>, 2> frac {}, tapGain {};
    Smoothed lenSm;
};

// ---------------------------------------------------------------- Tape Reverse
class TapeReverse
{
public:
    void prepare (double sampleRate, double maxLenMs)
    {
        fs = sampleRate;
        const int maxLen = (int) (maxLenMs * 0.001 * fs) + 8;
        for (auto& b : buf) b.allocate (2 * maxLen + 16);
        fdn.prepare (fs);
        setLength ((int) (0.6 * fs));
        configure (0.5, 0.6, 0.35);
        clear();
    }

    void clear()
    {
        for (auto& b : buf) b.clear();
        fdn.clear();
        phase = 0;
        for (auto& d : damp) d.reset();
    }

    // Length in samples. Changing it re-cuts the tape: the engine mutes the wet around the change.
    void setLength (int samples)
    {
        L = std::max (16, std::min (samples, (buf[0].capacity() - 8) / 2));
        inc = 1.0 / L;
    }
    int length() const { return L; }

    void configure (double swell, double size, double damping)
    {
        // a shorter decay relative to the chunk makes the reversed swell steeper and later
        const double rt = (L / fs) * (2.6 - 2.0 * clampd (swell, 0, 1));
        const double hz = 14000.0 * std::pow (1500.0 / 14000.0, clampd (damping, 0, 1));
        fdn.configure (0.5 + 1.5 * clampd (size, 0, 1), rt, hz);
        damp[0].set (fs, hz * 1.5); damp[1].set (fs, hz * 1.5);
    }

    inline void process (double inL, double inR, double& outL, double& outR)
    {
        double wl, wr;
        fdn.process (inL, inR, wl, wr);
        buf[0].push (damp[0].process (wl));
        buf[1].push (damp[1].process (wr));

        // two heads, half a chunk apart; each reads backwards (delay grows 0 -> 2L) under a sin^2 window
        phase += inc; if (phase >= 1.0) phase -= 1.0;
        double p2 = phase + 0.5; if (p2 >= 1.0) p2 -= 1.0;
        const double w1 = std::sin (kPi * phase), w2 = std::sin (kPi * p2);
        const double d1 = 2.0 * phase * L, d2 = 2.0 * p2 * L;
        outL = 1.6 * (w1 * w1 * buf[0].tapLin (d1) + w2 * w2 * buf[0].tapLin (d2));
        outR = 1.6 * (w1 * w1 * buf[1].tapLin (d1) + w2 * w2 * buf[1].tapLin (d2));
    }

private:
    double fs = 48000, phase = 0, inc = 0.001;
    int L = 1000;
    Fdn fdn;
    std::array<DelayLine, 2> buf;
    std::array<OnePoleLP, 2> damp;
};
} // namespace vf
