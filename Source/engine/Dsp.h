// DSP building blocks for Velvet Fuzz. Plain C++17, scalar only (no SIMD intrinsics, no -march),
// so it runs on any x86-64 (macOS 11 Intel) or arm64 machine.
// Shared with Driftglass (same Biquad / OnePole / DelayLine / Allpass).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vf
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) { return 20.0 * std::log10 (std::max (g, 1e-9)); }
inline double clampd (double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline double lerp (double a, double b, double t) { return a + (b - a) * t; }
inline double softClip (double x) { return std::tanh (x); }

// RBJ biquad, transposed direct form II.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void norm (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    void bypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    static double w0 (double fs, double fc) { return kTwoPi * clampd (fc, 5.0, fs * 0.47) / fs; }

    void lowpass (double fs, double fc, double q)
    {
        const double w = w0 (fs, fc), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double fc, double q)
    {
        const double w = w0 (fs, fc), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandpass (double fs, double fc, double q)
    {
        const double w = w0 (fs, fc), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak (double fs, double fc, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (fs, fc), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double fs, double fc, double db, double S = 0.8)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (fs, fc), c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sa = 2 * std::sqrt (A) * al;
        norm (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
              (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void highShelf (double fs, double fc, double db, double S = 0.8)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (fs, fc), c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sa = 2 * std::sqrt (A) * al;
        norm (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
              (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

struct OnePoleLP
{
    double a = 0, z = 0;
    void set (double fs, double fc) { a = std::exp (-kTwoPi * clampd (fc, 1.0, fs * 0.49) / fs); }
    void setTime (double fs, double seconds) { a = seconds <= 0 ? 0 : std::exp (-1.0 / (seconds * fs)); }
    inline double process (double x) { z = x + a * (z - x); return z; }
    void reset (double v = 0) { z = v; }
};

struct DcBlocker
{
    double x1 = 0, y1 = 0, r = 0.9995;
    void set (double fs, double fc = 8.0) { r = std::exp (-kTwoPi * fc / fs); }
    void reset() { x1 = y1 = 0; }
    inline double process (double x) { const double y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

// Per-sample parameter smoother (one-pole towards a target).
struct Smoothed
{
    double v = 0, target = 0, a = 0.999;
    void prepare (double fs, double seconds, double initial) { a = std::exp (-1.0 / (seconds * fs)); v = target = initial; }
    void set (double t) { target = t; }
    void snap() { v = target; }
    inline double next() { v = target + a * (v - target); return v; }
};

// Deterministic xorshift RNG (no allocation, real-time safe).
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline double uni() { return (next() >> 8) * (1.0 / 16777216.0); }   // [0,1)
    inline double bi() { return uni() * 2.0 - 1.0; }                      // [-1,1)
};

// Smoothly wandering random value, normalised to roughly unit RMS.
struct Drift
{
    OnePoleLP a, b;
    Rng rng;
    double scale = 1.0;
    void set (double fs, double fc, uint32_t seed)
    {
        a.set (fs, fc); b.set (fs, fc); rng = Rng (seed);
        a.reset(); b.reset();
        // analytic RMS of white noise (variance 1/3) through two identical one-poles
        // h[n] = (1-p)^2 (n+1) p^n  ->  sum h^2 = (1-p)^4 (1+p^2) / (1-p^2)^3
        const double p = a.a, q = 1.0 - p * p;
        const double var2 = (1.0 / 3.0) * std::pow (1.0 - p, 4.0) * (1.0 + p * p) / (q * q * q);
        scale = var2 > 1e-18 ? 1.0 / std::sqrt (var2) : 1.0;
    }
    inline double process() { return clampd (b.process (a.process (rng.bi())) * scale, -3.0, 3.0); }
};

struct Lfo
{
    double ph = 0, inc = 0;
    void set (double fs, double hz) { inc = hz / fs; }
    void setPhase (double p) { ph = p - std::floor (p); }
    inline void tick() { ph += inc; if (ph >= 1.0) ph -= 1.0; }
    inline double sine() const { return std::sin (kTwoPi * ph); }
    inline double sineAt (double offset) const { return std::sin (kTwoPi * (ph + offset)); }
    inline double triAt (double offset) const
    {
        double p = ph + offset; p -= std::floor (p);
        return p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p;
    }
};

// Circular delay line, power-of-two length. tap(0) is the most recently pushed sample.
struct DelayLine
{
    std::vector<double> buf;
    int w = 0, mask = 0;
    void allocate (int maxLen)
    {
        int n = 1; while (n < maxLen + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0); mask = n - 1; w = 0;
    }
    int capacity() const { return mask - 3; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0); }
    inline void push (double x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline double tap (int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }
    inline double tapLin (double d) const
    {
        const int i = (int) d; const double f = d - i;
        return tap (i) + (tap (i + 1) - tap (i)) * f;
    }
    // 4-point Hermite interpolation, for modulated reads.
    inline double tapHermite (double d) const
    {
        d = std::max (1.0, d);
        const int i = (int) d; const double f = d - i;
        const double xm1 = tap (i - 1), x0 = tap (i), x1 = tap (i + 1), x2 = tap (i + 2);
        const double c1 = 0.5 * (x1 - xm1);
        const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};

// Schroeder allpass with an optional modulated delay.
struct Allpass
{
    DelayLine line;
    double len = 100;
    void allocate (int maxLen) { line.allocate (maxLen + 8); }
    void clear() { line.clear(); }
    inline double process (double x, double g, double mod = 0.0)
    {
        const double dl = std::fabs (mod) < 1e-12 ? line.tap ((int) len - 1) : line.tapHermite (len - 1 + mod);
        const double v = x + g * dl;
        line.push (v);
        return dl - g * v;
    }
};

// Attack/release envelope follower on |x|.
struct EnvFollower
{
    double env = 0, att = 0, rel = 0;
    void set (double fs, double attackMs, double releaseMs)
    {
        att = std::exp (-1.0 / (std::max (0.01, attackMs) * 0.001 * fs));
        rel = std::exp (-1.0 / (std::max (0.01, releaseMs) * 0.001 * fs));
    }
    void reset() { env = 0; }
    inline double process (double x)
    {
        const double a = std::fabs (x);
        env = a + (a > env ? att : rel) * (env - a);
        return env;
    }
};

// Delay-line pitch shifter: two taps sweeping through a window, Hann crossfaded.
// ratio > 1 raises the pitch. Zero reported latency (the taps run behind the write head).
struct PitchShifter
{
    DelayLine line;
    double phase = 0, window = 2048, base = 64;
    void prepare (double fs, double windowMs = 45.0, double maxBaseMs = 80.0)
    {
        window = std::max (64.0, windowMs * 0.001 * fs);
        line.allocate ((int) (window + maxBaseMs * 0.001 * fs) + 16);
        phase = 0;
    }
    void clear() { line.clear(); phase = 0; }
    inline double process (double x, double ratio, double baseDelaySamples = 0.0)
    {
        line.push (x);
        phase += (1.0 - ratio) / window;
        phase -= std::floor (phase);
        double p2 = phase + 0.5; if (p2 >= 1.0) p2 -= 1.0;
        const double b = std::min (std::max (2.0, baseDelaySamples), (double) line.capacity() - window - 4);
        const double w1 = 0.5 - 0.5 * std::cos (kTwoPi * phase);
        const double w2 = 0.5 - 0.5 * std::cos (kTwoPi * p2);
        return w1 * line.tapHermite (b + phase * window) + w2 * line.tapHermite (b + p2 * window);
    }
};

inline bool isBad (double x) { return ! (std::fabs (x) < 1e6); }   // catches NaN and inf
} // namespace vf
