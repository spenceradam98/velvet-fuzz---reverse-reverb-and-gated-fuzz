#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

class VelvetFuzzProcessor : public juce::AudioProcessor
{
public:
    VelvetFuzzProcessor();
    ~VelvetFuzzProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentProgram); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- presets (factory + user files)
    void loadFactoryPreset (int index);
    bool loadUserPreset (const juce::File&);
    bool saveUserPreset (const juce::String& name);
    juce::Array<juce::File> userPresets() const;
    static juce::File userPresetFolder();
    juce::String getPresetName() const { return presetName; }
    void stepPreset (int delta);   // walks factory presets, then user presets

    // ---- UI state saved with the session
    int themeIndex = 0;
    float uiScale = 1.0f;

    const vf::Engine& getEngine() const { return engine; }
    int getLengthMs() const { return (int) std::lround (engine.lengthMs()); }
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void applyValues (const std::array<float, vf::NumParams>&);

    vf::Engine engine;
    std::array<std::atomic<float>*, vf::NumParams> raw {};
    std::vector<float> scratchL, scratchR;
    int currentProgram = 0;
    juce::String presetName { "Glide Guitar" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelvetFuzzProcessor)
};
