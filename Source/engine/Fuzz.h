// The fuzz: two transistor stages with a starving bias and a sagging supply, oversampled,
// followed by tone, mids and a downward-expander gate.
#pragma once

#include "Dsp.h"
#include <array>

namespace vf
{
// Transistor transfer curve: 0 below cutoff (u <= 0), a smoothstep through the active region,
// flat at saturation (u >= 2). Its slope is zero at both ends, so a stage biased near cutoff
// swallows small signals (the sputter / "velcro" gating) and a hot signal clips softly.
inline double xferG (double u)
{
    if (u <= 0.0) return 0.0;
    if (u >= 2.0) return 1.0;
    const double t = 0.5 * u;
    return t * t * (3.0 - 2.0 * t);
}

// Antiderivative of xferG (continuous at 0 and 2), for first-order ADAA.
inline double xferGInt (double u)
{
    if (u <= 0.0) return 0.0;
    if (u >= 2.0) return u - 1.0;
    const double t = 0.5 * u, t3 = t * t * t;
    return 2.0 * (t3 - 0.5 * t3 * t);
}

// First-order antiderivative anti-aliasing around xferG.
struct AdaaStage
{
    double uPrev = 0, gPrev = 0;
    void reset (double u = 0) { uPrev = u; gPrev = xferGInt (u); }
    inline double process (double u)
    {
        const double gi = xferGInt (u), du = u - uPrev;
        const double y = std::fabs (du) > 1e-7 ? (gi - gPrev) / du : xferG (0.5 * (u + uPrev));
        uPrev = u; gPrev = gi;
        return y;
    }
};

// Butterworth low-pass as a cascade of biquads, used for up/down sampling.
struct ButterLp
{
    static constexpr int kSections = 6;   // 12th order
    std::array<Biquad, kSections> s;
    void set (double fs, double fc)
    {
        for (int k = 0; k < kSections; ++k)
            s[(size_t) k].lowpass (fs, fc, 1.0 / (2.0 * std::sin ((2 * k + 1) * kPi / (4.0 * kSections))));
    }
    void reset() { for (auto& b : s) b.reset(); }
    inline double process (double x) { for (auto& b : s) x = b.process (x); return x; }
};

// One channel of the fuzz core. Runs at the oversampled rate.
struct FuzzCore
{
    AdaaStage st1, st2;
    DcBlocker couple;   // interstage coupling cap
    EnvFollower sagEnv;
    void prepare (double fsOs)
    {
        couple.set (fsOs, 40.0);
        sagEnv.set (fsOs, 4.0, 160.0);
        reset();
    }
    void reset() { st1.reset(); st2.reset(); couple.reset(); sagEnv.reset(); primed = false; }
    bool primed = false;

    // gain1: stage 1 drive, bias01: 0 starved .. 1 healthy, sag01: supply starve amount.
    // Returns the stage output and writes the live supply voltage.
    inline double process (double x, double gain1, double bias01, double sag01, double& volts)
    {
        const double v = clampd (9.0 - 4.0 * sag01 - 4.5 * sag01 * sagEnv.env, 3.0, 9.0);
        volts = v;
        const double h = v / 9.0;                                   // headroom shrinks with the rail
        const double b1 = (0.12 + 0.88 * bias01) * (0.55 + 0.45 * h);   // the rail drop starves the bias too
        const double b2 = 1.0 * (0.75 + 0.25 * h);

        if (! primed) { st1.reset (b1); st2.reset (b2); primed = true; }   // start at rest: no thump
        double y = st1.process (x * gain1 + b1) - xferG (b1);
        y = couple.process (y);
        y = st2.process (y * 3.5 + b2) - xferG (b2);
        sagEnv.process (y);
        return y * h * 1.6;
    }
};

// Downward expander (gate). Keyed by the fuzz input so it follows the playing, not the clipped output.
struct Gate
{
    EnvFollower env;
    double gain = 1, att = 0, rel = 0, thr = 1e-5, ratio = 3;
    bool active = false;
    void prepare (double fs)
    {
        env.set (fs, 0.5, 45.0);
        att = std::exp (-1.0 / (0.0015 * fs));   // opening
        rel = std::exp (-1.0 / (0.030 * fs));    // closing
        reset();
    }
    void reset() { env.reset(); gain = 1; }
    // amount 0..1, bias 0..1 (starved settings choke earlier)
    void set (double amount, double bias01)
    {
        active = amount > 0.001;
        thr = dbToGain (-72.0 + 50.0 * amount + 8.0 * (1.0 - bias01) * amount);
    }
    inline double process (double x, double key)
    {
        const double e = env.process (key);
        double target = 1.0;
        if (active && e < thr) target = std::max (1e-4, std::pow (std::max (e, 1e-9) / thr, ratio - 1.0));
        gain = target + (target > gain ? att : rel) * (gain - target);
        return x * gain;
    }
};

// The whole fuzz pedal, stereo, at the host rate. Oversampling is internal.
class Fuzz
{
public:
    void prepare (double sampleRate, int osFactor)
    {
        fs = sampleRate;
        setOversampling (osFactor);
        for (auto& g : gate) g.prepare (fs);
        for (auto& d : dc) d.set (fs, 12.0);
        smGain.prepare (fs, 0.03, gain1); smBias.prepare (fs, 0.05, bias); smSag.prepare (fs, 0.05, sag);
        for (auto& f : inHp) f.highpass (fs, 70.0, 0.7);
        configure (0.6, 0.75, 0.2, 0.15, 0.5, 0.0);
        reset();
    }
    void snapControls() { smGain.snap(); smBias.snap(); smSag.snap(); }

    void setOversampling (int factor)
    {
        os = factor >= 4 ? 4 : 2;
        const double fsOs = fs * os;
        for (int c = 0; c < 2; ++c)
        {
            core[(size_t) c].prepare (fsOs);
            up[(size_t) c].set (fsOs, std::min (0.45 * fs, 20000.0));
            down[(size_t) c].set (fsOs, std::min (0.45 * fs, 20000.0));
        }
        resetFilters();
    }
    int oversampling() const { return os; }

    void reset()
    {
        resetFilters();
        for (auto& g : gate) g.reset();
        for (auto& d : dc) d.reset();
        for (auto& f : inHp) f.reset();
        for (auto& f : tone) f.reset();
        for (auto& f : mids) f.reset();
    }

    void configure (double gain01, double bias01, double sag01, double gate01, double tone01, double midsAmt)
    {
        gain1 = dbToGain (6.0 + 40.0 * clampd (gain01, 0, 1));
        bias = clampd (bias01, 0, 1);
        sag = clampd (sag01, 0, 1);
        smGain.set (gain1); smBias.set (bias); smSag.set (sag);
        for (auto& g : gate) g.set (gate01, bias);
        const double fc = 700.0 * std::pow (12000.0 / 700.0, clampd (tone01, 0, 1));
        const double midDb = midsAmt < 0 ? midsAmt * 14.0 : midsAmt * 8.0;
        for (int c = 0; c < 2; ++c)
        {
            tone[(size_t) c].lowpass (fs, fc, 0.6);
            mids[(size_t) c].peak (fs, 750.0, 0.7, midDb);
        }
    }

    // In-place stereo sample. Returns the fuzzed sample pair.
    inline void process (double& l, double& r)
    {
        double* io[2] = { &l, &r };
        double voltsSum = 0;
        const double g1 = smGain.next(), bi = smBias.next(), sg = smSag.next();
        for (int c = 0; c < 2; ++c)
        {
            const double x = inHp[(size_t) c].process (*io[c]);
            double y = 0;
            for (int k = 0; k < os; ++k)
            {
                double v;
                const double xu = up[(size_t) c].process (k == 0 ? x * os : 0.0);
                y = down[(size_t) c].process (core[(size_t) c].process (xu, g1, bi, sg, v));
                voltsSum += v;
            }
            y = dc[(size_t) c].process (y);
            y = mids[(size_t) c].process (tone[(size_t) c].process (y));
            *io[c] = gate[(size_t) c].process (y, x);
        }
        volts = voltsSum / (2.0 * os);
    }

    double volts = 9.0;   // live supply voltage, for the meter
    double gateGain() const { return std::min (gate[0].gain, gate[1].gain); }

private:
    void resetFilters()
    {
        for (int c = 0; c < 2; ++c) { up[(size_t) c].reset(); down[(size_t) c].reset(); core[(size_t) c].reset(); }
    }

    double fs = 48000, gain1 = 4, bias = 0.75, sag = 0.2;
    int os = 2;
    std::array<FuzzCore, 2> core;
    std::array<ButterLp, 2> up, down;
    std::array<Gate, 2> gate;
    std::array<DcBlocker, 2> dc;
    std::array<Biquad, 2> inHp, tone, mids;
    Smoothed smGain, smBias, smSag;
};
} // namespace vf
