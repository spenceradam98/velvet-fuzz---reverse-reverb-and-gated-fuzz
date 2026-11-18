// Renders the editor in every theme to PNG files (for docs). Build with -DVELVETFUZZ_BUILD_SNAPSHOTS=ON;
// on Linux run under a display (e.g. xvfb-run). Usage: velvetfuzz_snapshots <out-dir> [preset-index] [scale]
#include "../Source/plugin/PluginProcessor.h"
#include "../Source/plugin/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    dir.createDirectory();
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 1.0f;
    VelvetFuzzProcessor proc;
    proc.setCurrentProgram (argc > 2 ? juce::String (argv[2]).getIntValue() : 0);
    proc.prepareToPlay (48000, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < 60; ++b)   // run some audio so meters, the voltmeter and the swell show something
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i) buf.setSample (c, i, 0.35f * std::sin ((float) (b * 512 + i) * 0.025f));
        proc.processBlock (buf, midi);
    }
    const auto& list = themes();
    for (int t = 0; t < (int) list.size(); ++t)
    {
        proc.themeIndex = t;
        proc.uiScale = 1.0f;
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (VelvetFuzzContent::W, VelvetFuzzContent::H);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
        const auto name = list[(size_t) t].name.replace (" ", "-").toLowerCase();
        juce::FileOutputStream out (dir.getChildFile (juce::String (t) + "-" + name + ".png"));
        out.setPosition (0); out.truncate();
        juce::PNGImageFormat().writeImageToStream (img, out);
    }
    return 0;
}
