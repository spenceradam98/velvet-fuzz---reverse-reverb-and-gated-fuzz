// Velvet Fuzz engine: reverse reverb + gated fuzz with switchable order. Plain C++17, no JUCE,
// so it can be unit tested.
#pragma once

#include "Fuzz.h"
#include "Params.h"
#include "Reverse.h"
#include <array>
#include <atomic>
#include <string>
#include <utility>
#include <vector>

namespace vf
{
class Engine
{
public:
    static constexpr double kMaxLenMs = 2000.0;

    Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    // Parameter values in their natural units (as in the ParamDef table).
    void setParam (int index, float value) { if (index >= 0 && index < NumParams) values[(size_t) index] = value; }
    float getParam (int index) const { return values[(size_t) index]; }
    void setTempo (double bpm) { if (bpm > 20 && bpm < 400) tempo = bpm; }

    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    // Latency the host should compensate: the Length in Tape Reverse mode, otherwise zero.
    // Computed from the current parameters, so it is right before the first process() call.
    int latencySamples() const;

    double lengthMs() const;   // resolved reverse length (free or tempo synced)

    // Metering for the UI (written on the audio thread).
    std::atomic<float> inLevel { 0 }, outLevel { 0 }, supplyVolts { 9 }, wetLevel { 0 }, gateLevel { 1 };

private:
    void update();
    inline void verbSection (double inL, double inR, double& dryL, double& dryR, double& wetL, double& wetR);

    double fs = 48000, tempo = 120;
    std::array<float, NumParams> values {};

    GatedReverse gated;
    TapeReverse tape;
    Fuzz fuzz;
    DelayLine preL, preR;        // reverb pre-delay
    DelayLine alignL, alignR;    // dry delay for Tape Reverse latency
    DelayLine bypL, bypR;        // latency-aligned input for bypass

    // what is running now (changes are applied under a short mute so they never click)
    int activeOrder = 0, activeMode = 0, activeTapeLen = 0, activeOs = 2;
    Smoothed transition, verbOn, fuzzOn, mix, inTrim, outGain, width, bypass, preDelay, fuzzLevel;
    float meterIn = 0, meterOut = 0, meterWet = 0;
};

// ---------------------------------------------------------------- Presets
struct Preset
{
    std::string name;
    std::string bank;
    std::vector<std::pair<std::string, float>> values;   // overrides on top of the parameter defaults
};

const std::vector<Preset>& factoryPresets();
const std::vector<std::string>& presetBanks();

// Full parameter vector for a preset (defaults + overrides).
std::array<float, NumParams> presetValues (const Preset&);
} // namespace vf
