#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Themes.h"

// A metal stomp switch that toggles a pedal on and off.
class FootSwitch : public juce::Button
{
public:
    FootSwitch() : juce::Button ("footswitch") { setClickingTogglesState (true); }
    void setTheme (const Theme* th) { t = th; repaint(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    const Theme* t = nullptr;
};

// All controls live in a fixed-size content component that is scaled to the window size.
// The look is a small pedalboard: two stompboxes whose positions follow the Order, joined by a
// patch cable you click to change the order.
class VelvetFuzzContent : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1000, H = 660;

    explicit VelvetFuzzContent (VelvetFuzzProcessor&);
    ~VelvetFuzzContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    struct Knob
    {
        int param = 0;
        juce::String label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    struct Pedal
    {
        juce::String title, subtitle;
        int onParam = 0;
        std::vector<Knob*> knobs;
        int cols = 3;
        juce::Rectangle<int> bounds, display;
        FootSwitch foot;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> footAttach;
    };

    Knob* addKnob (int param, const char* label);
    void setupBox (juce::ComboBox&, int param, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&);
    void layoutPedal (Pedal&, juce::Rectangle<int>);
    void applyTheme (int index);
    void showPresetMenu();
    void savePresetDialog();
    void timerCallback() override;
    int order() const;
    juce::Path cablePath() const;

    void drawPedal (juce::Graphics&, const Pedal&, bool isFuzz);
    void drawSwellDisplay (juce::Graphics&, juce::Rectangle<float>);
    void drawVoltmeter (juce::Graphics&, juce::Rectangle<float>);
    void drawCable (juce::Graphics&, juce::Point<float> a, juce::Point<float> b, juce::Colour, float sag);
    void drawJack (juce::Graphics&, juce::Point<float>);
    void drawMeters (juce::Graphics&, juce::Rectangle<float>);

    VelvetFuzzProcessor& proc;
    VelvetLookAndFeel lnf;
    std::vector<std::unique_ptr<Knob>> knobs;
    Pedal verb, fuzz;
    std::vector<Knob*> globalKnobs;
    Knob* lengthKnob = nullptr;

    juce::ComboBox modeBox, syncBox, orderBox, osBox, themeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach, syncAttach, orderAttach, osAttach;
    juce::ToggleButton bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;
    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" };

    juce::Image background;
    juce::Rectangle<float> meterArea, cableHit;
    float inMeter = 0, outMeter = 0, volts = 9.0f, wet = 0, gateOpen = 1;
    bool cableHover = false;
    int shownOrder = -1;
    juce::String shownPreset;
    std::unique_ptr<juce::AlertWindow> saveWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelvetFuzzContent)
};

class VelvetFuzzEditor : public juce::AudioProcessorEditor
{
public:
    explicit VelvetFuzzEditor (VelvetFuzzProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }

private:
    VelvetFuzzProcessor& proc;
    VelvetFuzzContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelvetFuzzEditor)
};
