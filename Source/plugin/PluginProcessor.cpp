#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
String formatValue (const vf::ParamDef& d, float v)
{
    const String unit (d.unit);
    if (unit == "dB") return (v > 0 ? "+" : "") + String (v, 1) + " dB";
    if (unit == "Hz") return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (v, v < 10.0f ? 2 : 0) + " Hz";
    if (unit == "ms") return String (roundToInt (v)) + " ms";
    if (unit == "s") return String (v, v < 10.0f ? 1 : 0) + " s";
    if (unit == "mids")
    {
        if (std::abs (v) < 0.03f) return String ("Flat");
        return (v < 0 ? "Scoop " : "Push ") + String (roundToInt (std::abs (v) * 100.0f)) + "%";
    }
    return String (roundToInt (v * 100.0f)) + "%";
}
} // namespace

AudioProcessorValueTreeState::ParameterLayout VelvetFuzzProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const auto* defs = vf::paramDefs();
    for (int i = 0; i < vf::NumParams; ++i)
    {
        const auto& d = defs[i];
        const ParameterID pid { d.id, 1 };
        switch (d.kind)
        {
            case vf::Kind::Bool:
                p.push_back (std::make_unique<AudioParameterBool> (pid, d.name, d.def > 0.5f));
                break;
            case vf::Kind::Choice:
                p.push_back (std::make_unique<AudioParameterChoice> (pid, d.name, StringArray::fromTokens (d.choices, "|", ""), (int) d.def));
                break;
            case vf::Kind::Float:
            default:
            {
                NormalisableRange<float> range (d.min, d.max);
                if (d.centre > d.min && d.centre < d.max) range.setSkewForCentre (d.centre);
                const auto attrs = AudioParameterFloatAttributes()
                                       .withLabel (d.unit)
                                       .withStringFromValueFunction ([d] (float v, int) { return formatValue (d, v); });
                p.push_back (std::make_unique<AudioParameterFloat> (pid, d.name, range, d.def, attrs));
                break;
            }
        }
    }
    return { p.begin(), p.end() };
}

VelvetFuzzProcessor::VelvetFuzzProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Velvet Fuzz", createLayout())
{
    const auto* defs = vf::paramDefs();
    for (int i = 0; i < vf::NumParams; ++i) raw[(size_t) i] = apvts.getRawParameterValue (defs[i].id);
    loadFactoryPreset (0);
}

bool VelvetFuzzProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet(), in = layouts.getMainInputChannelSet();
    const bool okOut = out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
    const bool okIn = in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
    return okOut && okIn;
}

void VelvetFuzzProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < vf::NumParams; ++i) engine.setParam (i, raw[(size_t) i]->load());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latencySamples());
    scratchL.assign ((size_t) jmax (1, samplesPerBlock), 0.0f);
    scratchR.assign ((size_t) jmax (1, samplesPerBlock), 0.0f);
}

void VelvetFuzzProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    if (n == 0 || outCh == 0) return;
    if ((int) scratchL.size() < n) { scratchL.resize ((size_t) n); scratchR.resize ((size_t) n); }   // hosts may exceed the announced size

    for (int i = 0; i < vf::NumParams; ++i) engine.setParam (i, raw[(size_t) i]->load());
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) engine.setTempo (*bpm);

    // Tape Reverse reports its Length as latency (the dry is delayed inside the engine to match)
    if (const int lat = engine.latencySamples(); lat != getLatencySamples()) setLatencySamples (lat);

    const float* inL = inCh > 0 ? buffer.getReadPointer (0) : scratchL.data();
    const float* inR = inCh > 1 ? buffer.getReadPointer (1) : inL;
    if (inCh == 0) std::fill (scratchL.begin(), scratchL.begin() + n, 0.0f);

    // the engine reads both inputs before writing, so process into scratch and copy out
    engine.process (inL, inR, scratchL.data(), scratchR.data(), n);

    if (outCh == 1)
    {
        auto* o = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) o[i] = 0.5f * (scratchL[(size_t) i] + scratchR[(size_t) i]);
    }
    else
    {
        buffer.copyFrom (0, 0, scratchL.data(), n);
        buffer.copyFrom (1, 0, scratchR.data(), n);
        for (int c = 2; c < outCh; ++c) buffer.clear (c, 0, n);
    }
}

AudioProcessorParameter* VelvetFuzzProcessor::getBypassParameter() const { return apvts.getParameter ("bypass"); }

// ---------------------------------------------------------------- presets
int VelvetFuzzProcessor::getNumPrograms() { return (int) vf::factoryPresets().size(); }

const String VelvetFuzzProcessor::getProgramName (int index)
{
    const auto& ps = vf::factoryPresets();
    return isPositiveAndBelow (index, (int) ps.size()) ? String (ps[(size_t) index].name) : String();
}

void VelvetFuzzProcessor::setCurrentProgram (int index) { loadFactoryPreset (index); }

void VelvetFuzzProcessor::applyValues (const std::array<float, vf::NumParams>& v)
{
    const auto* defs = vf::paramDefs();
    for (int i = 0; i < vf::NumParams; ++i)
        if (i == vf::Bypass) continue;   // a preset never bypasses (or un-bypasses) the plugin
        else if (auto* prm = apvts.getParameter (defs[i].id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v[(size_t) i]));
}

void VelvetFuzzProcessor::loadFactoryPreset (int index)
{
    const auto& ps = vf::factoryPresets();
    if (! isPositiveAndBelow (index, (int) ps.size())) return;
    currentProgram = index;
    presetName = ps[(size_t) index].name;
    applyValues (vf::presetValues (ps[(size_t) index]));
}

File VelvetFuzzProcessor::userPresetFolder()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Velvet Fuzz").getChildFile ("Presets");
}

Array<File> VelvetFuzzProcessor::userPresets() const
{
    auto files = userPresetFolder().findChildFiles (File::findFiles, false, "*.vfpreset");
    files.sort();
    return files;
}

bool VelvetFuzzProcessor::saveUserPreset (const String& name)
{
    const auto clean = File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    XmlElement xml ("VelvetFuzzPreset");
    xml.setAttribute ("name", name.trim());
    xml.setAttribute ("version", 1);
    const auto* defs = vf::paramDefs();
    for (int i = 0; i < vf::NumParams; ++i)
    {
        auto* e = xml.createNewChildElement ("param");
        e->setAttribute ("id", defs[i].id);
        e->setAttribute ("value", (double) raw[(size_t) i]->load());
    }
    userPresetFolder().createDirectory();
    if (! xml.writeTo (userPresetFolder().getChildFile (clean + ".vfpreset"))) return false;
    presetName = name.trim();
    currentProgram = -1;
    return true;
}

bool VelvetFuzzProcessor::loadUserPreset (const File& f)
{
    auto xml = XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("VelvetFuzzPreset")) return false;
    auto v = vf::presetValues (vf::Preset { "", "", {} });
    for (auto* e : xml->getChildWithTagNameIterator ("param"))
    {
        const int idx = vf::paramIndex (e->getStringAttribute ("id").toRawUTF8());
        if (idx >= 0) v[(size_t) idx] = (float) e->getDoubleAttribute ("value", v[(size_t) idx]);
    }
    applyValues (v);
    presetName = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
    currentProgram = -1;
    return true;
}

void VelvetFuzzProcessor::stepPreset (int delta)
{
    const int nf = getNumPrograms();
    const auto users = userPresets();
    const int total = nf + users.size();
    int cur = currentProgram;
    if (cur < 0)
    {
        cur = nf;
        for (int i = 0; i < users.size(); ++i)
            if (users[i].getFileNameWithoutExtension() == File::createLegalFileName (presetName)) cur = nf + i;
    }
    const int next = ((cur + delta) % total + total) % total;
    if (next < nf) loadFactoryPreset (next);
    else loadUserPreset (users[next - nf]);
}

// ---------------------------------------------------------------- state
void VelvetFuzzProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("theme", themeIndex, nullptr);
    state.setProperty ("uiScale", uiScale, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void VelvetFuzzProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            presetName = tree.getProperty ("presetName", presetName).toString();
            themeIndex = tree.getProperty ("theme", 0);
            uiScale = (float) (double) tree.getProperty ("uiScale", 1.0);
            apvts.replaceState (tree);
        }
}

AudioProcessorEditor* VelvetFuzzProcessor::createEditor() { return new VelvetFuzzEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VelvetFuzzProcessor(); }
