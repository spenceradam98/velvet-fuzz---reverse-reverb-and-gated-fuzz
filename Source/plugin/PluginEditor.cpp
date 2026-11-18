#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr int kPedalW = 370, kPedalH = 462, kPedalY = 86, kSideX = 38, kJackDY = 74;
constexpr float kStart = MathConstants<float>::pi * -0.78f, kEnd = MathConstants<float>::pi * 0.78f;

Font titleFont (float size) { return Font (FontOptions (size, Font::bold)).withExtraKerningFactor (0.18f); }
bool lightPanel (const Theme& t) { return t.panel.getPerceivedBrightness() > 0.6f && t.panel.getFloatAlpha() > 0.8f; }
}

// ============================================================================ footswitch
void FootSwitch::paintButton (Graphics& g, bool highlighted, bool down)
{
    if (t == nullptr) return;
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const float d = jmin (r.getWidth(), r.getHeight());
    auto c = Rectangle<float> (d, d).withCentre (r.getCentre());
    // hex nut
    Path nut;
    nut.addPolygon (c.getCentre(), 6, d * 0.5f, MathConstants<float>::pi / 6.0f);
    g.setColour (Colours::black.withAlpha (0.35f)); g.fillPath (nut, AffineTransform::translation (1.5f, 2.5f));
    g.setGradientFill (ColourGradient (Colour (0xffe9e9ea), c.getX(), c.getY(), Colour (0xff77787d), c.getRight(), c.getBottom(), false));
    g.fillPath (nut);
    // the button cap
    auto cap = c.reduced (d * (down ? 0.2f : 0.17f));
    g.setGradientFill (ColourGradient (Colour (0xfffafafa), cap.getX(), cap.getY(), Colour (0xff8d8e93), cap.getRight(), cap.getBottom(), true));
    g.fillEllipse (cap);
    g.setColour (Colours::black.withAlpha (0.35f)); g.drawEllipse (cap, 1.0f);
    g.setColour (Colours::white.withAlpha (0.7f)); g.drawEllipse (cap.reduced (d * 0.06f), 1.0f);
    if (highlighted) { g.setColour (t->accent.withAlpha (0.35f)); g.drawEllipse (c.expanded (2), 2.0f); }
}

// ============================================================================ content
VelvetFuzzContent::VelvetFuzzContent (VelvetFuzzProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    using namespace vf;
    const auto* defs = paramDefs();

    verb.title = "REVERSE"; verb.subtitle = "backwards swell"; verb.onParam = VerbOn; verb.cols = 3;
    for (auto kv : { std::pair<int, const char*> { VerbLen, "Length" }, { VerbSwell, "Swell" }, { VerbSize, "Size" },
                     { VerbDamp, "Damping" }, { VerbPre, "Pre-delay" }, { VerbMix, "Mix" } })
        verb.knobs.push_back (addKnob (kv.first, kv.second));
    fuzz.title = "FUZZ"; fuzz.subtitle = "starved & gated"; fuzz.onParam = FuzzOn; fuzz.cols = 4;
    for (auto kv : { std::pair<int, const char*> { FuzzGain, "Gain" }, { FuzzBias, "Bias" }, { FuzzSag, "Sag" }, { FuzzGate, "Gate" },
                     { FuzzTone, "Tone" }, { FuzzMids, "Mids" }, { FuzzLevel, "Level" } })
        fuzz.knobs.push_back (addKnob (kv.first, kv.second));
    for (auto kv : { std::pair<int, const char*> { InTrim, "Input" }, { Width, "Width" }, { OutGain, "Output" } })
        globalKnobs.push_back (addKnob (kv.first, kv.second));
    for (auto& k : knobs)
    {
        if (k->param == VerbLen) lengthKnob = k.get();
        if (k->param == InTrim || k->param == OutGain || k->param == FuzzLevel || k->param == FuzzMids)
            k->slider.getProperties().set ("bipolar", true);
    }

    for (auto* pd : { &verb, &fuzz })
    {
        pd->foot.setTooltip ("Switch " + pd->title.toLowerCase() + " on or off");
        addAndMakeVisible (pd->foot);
        pd->footAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, defs[pd->onParam].id, pd->foot);
    }

    setupBox (modeBox, VerbMode, modeAttach);
    setupBox (syncBox, VerbSync, syncAttach);
    setupBox (orderBox, Order, orderAttach);
    setupBox (osBox, Oversample, osAttach);
    modeBox.setTooltip ("Gated Reverse: zero latency, playable live. Tape Reverse: true reversed tail, adds latency equal to Length.");
    syncBox.setTooltip ("Sync Length to the host tempo");
    orderBox.setTooltip ("Signal order. You can also click the patch cable.");
    osBox.setTooltip ("Fuzz oversampling. 2x is lighter on older machines.");

    bypassButton.setTooltip ("Bypass (latency stays the same, so it switches cleanly)");
    addAndMakeVisible (bypassButton);
    bypassAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypassButton);

    presetButton.onClick = [this] { showPresetMenu(); };
    prevButton.onClick = [this] { proc.stepPreset (-1); };
    nextButton.onClick = [this] { proc.stepPreset (1); };
    saveButton.onClick = [this] { savePresetDialog(); };
    presetButton.setTooltip ("Browse presets");
    saveButton.setTooltip ("Save the current settings as a user preset");
    for (auto* b : { &presetButton, &prevButton, &nextButton, &saveButton }) addAndMakeVisible (*b);

    for (int i = 0; i < (int) themes().size(); ++i) themeBox.addItem (themes()[(size_t) i].name, i + 1);
    themeBox.onChange = [this] { applyTheme (themeBox.getSelectedId() - 1); };
    addAndMakeVisible (themeBox);

    setSize (W, H);
    applyTheme (jlimit (0, (int) themes().size() - 1, proc.themeIndex));
    themeBox.setSelectedId (proc.themeIndex + 1, dontSendNotification);
    timerCallback();   // show the preset name and meters straight away
    startTimerHz (30);
}

VelvetFuzzContent::~VelvetFuzzContent()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

VelvetFuzzContent::Knob* VelvetFuzzContent::addKnob (int param, const char* label)
{
    const auto* defs = vf::paramDefs();
    auto k = std::make_unique<Knob>();
    k->param = param;
    k->label = label;
    k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    k->slider.setRotaryParameters (MathConstants<float>::twoPi + kStart, MathConstants<float>::twoPi + kEnd, true);
    k->slider.setPopupDisplayEnabled (true, true, this, 1200);
    k->slider.setTooltip (defs[param].name);
    addAndMakeVisible (k->slider);
    k->attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, defs[param].id, k->slider);
    if (auto* prm = proc.apvts.getParameter (defs[param].id))
        k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

void VelvetFuzzContent::setupBox (ComboBox& box, int param, std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment>& att)
{
    const auto* defs = vf::paramDefs();
    box.addItemList (StringArray::fromTokens (defs[param].choices, "|", ""), 1);
    box.setJustificationType (Justification::centred);
    addAndMakeVisible (box);
    att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, defs[param].id, box);
}

int VelvetFuzzContent::order() const { return jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("order")->load()); }

void VelvetFuzzContent::applyTheme (int index)
{
    index = jlimit (0, (int) themes().size() - 1, index);
    proc.themeIndex = index;
    const auto& t = themes()[(size_t) index];
    lnf.setTheme (t);
    verb.foot.setTheme (&lnf.theme());
    fuzz.foot.setTheme (&lnf.theme());

    // pre-render the background at 2x: gradient, board texture, the theme's grain / scanlines
    background = Image (Image::ARGB, W * 2, H * 2, true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (2.0f));
    g.setGradientFill (ColourGradient (t.bgTop, 0, 0, t.bgBottom, (float) W * 0.3f, (float) H, false));
    g.fillAll();
    Random rnd (11);
    if (t.stars)
        for (int i = 0; i < 120; ++i)
        {
            const float x = rnd.nextFloat() * W, y = rnd.nextFloat() * 70.0f, s = 0.6f + rnd.nextFloat() * 1.4f;
            g.setColour (Colours::white.withAlpha (0.1f + 0.3f * rnd.nextFloat()));
            g.fillEllipse (x, y, s, s);
        }
    // the pedalboard: a darker plate with rails
    auto board = Rectangle<float> (14, (float) kPedalY - 12, (float) W - 28, (float) kPedalH + 24);
    g.setColour (Colours::black.withAlpha (0.28f));
    g.fillRoundedRectangle (board, 10.0f);
    g.setColour (Colours::black.withAlpha (0.18f));
    for (float y = board.getY() + 26; y < board.getBottom() - 10; y += 52)
        g.fillRect (board.getX() + 8, y, board.getWidth() - 16, 3.0f);
    if (t.glow)
    {
        g.setGradientFill (ColourGradient (t.accent.withAlpha (0.20f), W * 0.5f, H * 0.35f, t.accent.withAlpha (0.0f), W * 0.5f + 460, H * 0.35f + 300, true));
        g.fillAll();
    }
    if (t.grain)
        for (int i = 0; i < W * H / 6; ++i)
        {
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (t.grainAmount * rnd.nextFloat()));
            g.fillRect (rnd.nextFloat() * W, rnd.nextFloat() * H, 1.0f, 1.0f);
        }
    if (t.scanlines)
    {
        g.setColour (Colours::black.withAlpha (0.2f));
        for (int y = 0; y < H; y += 3) g.fillRect (0.0f, (float) y, (float) W, 1.0f);
    }
    for (auto* b : { &modeBox, &syncBox, &orderBox, &osBox }) b->setColour (ComboBox::textColourId, t.text);
    sendLookAndFeelChange();
    repaint();
}

// ---------------------------------------------------------------- layout
void VelvetFuzzContent::layoutPedal (Pedal& p, Rectangle<int> r)
{
    p.bounds = r;
    const bool isVerb = (&p == &verb);
    auto body = r.reduced (14, 0);
    if (isVerb)
    {
        auto row = Rectangle<int> (body.getX(), r.getY() + 58, body.getWidth(), 26);
        modeBox.setBounds (row.removeFromLeft (row.getWidth() * 3 / 5 - 4));
        row.removeFromLeft (8);
        syncBox.setBounds (row);
        p.display = { body.getX(), r.getY() + 92, body.getWidth(), 84 };
    }
    else
        p.display = { body.getX(), r.getY() + 58, body.getWidth(), 118 };

    auto grid = Rectangle<int> (body.getX(), r.getY() + 186, body.getWidth(), 196);
    const int rows = ((int) p.knobs.size() + p.cols - 1) / p.cols;
    const int cw = grid.getWidth() / p.cols, ch = grid.getHeight() / rows;
    const int knobSize = jmin (62, cw - 8, ch - 30);
    for (size_t k = 0; k < p.knobs.size(); ++k)
    {
        const int row = (int) k / p.cols, col = (int) k % p.cols;
        const int inRow = jmin (p.cols, (int) p.knobs.size() - row * p.cols);
        const int x0 = grid.getX() + (grid.getWidth() - inRow * cw) / 2;   // centre a short last row
        auto cell = Rectangle<int> (x0 + col * cw, grid.getY() + row * ch, cw, ch);
        p.knobs[k]->slider.setBounds (cell.withTrimmedBottom (28).withSizeKeepingCentre (knobSize, knobSize));
    }
    p.foot.setBounds (Rectangle<int> (64, 64).withCentre ({ r.getCentreX(), r.getBottom() - 46 }));
}

void VelvetFuzzContent::resized()
{
    const int hy = 18;
    prevButton.setBounds (372, hy, 30, 32);
    presetButton.setBounds (406, hy, 270, 32);
    nextButton.setBounds (680, hy, 30, 32);
    saveButton.setBounds (718, hy, 58, 32);
    themeBox.setBounds (850, hy + 2, 126, 28);

    const auto left = Rectangle<int> (kSideX, kPedalY, kPedalW, kPedalH);
    const auto right = Rectangle<int> (W - kSideX - kPedalW, kPedalY, kPedalW, kPedalH);
    const int ord = order();
    shownOrder = ord;
    layoutPedal (ord == 1 ? fuzz : verb, left);
    layoutPedal (ord == 1 ? verb : fuzz, right);

    const int gapX = left.getRight(), gapW = right.getX() - gapX;
    orderBox.setBounds (gapX + 10, kPedalY + 330, gapW - 20, 26);
    cableHit = Rectangle<float> ((float) gapX + 4, (float) kPedalY + 20, (float) gapW - 8, 220);

    // global strip
    const int sy = kPedalY + kPedalH + 22;
    for (size_t i = 0; i < globalKnobs.size(); ++i)
        globalKnobs[i]->slider.setBounds (44 + (int) i * 84, sy + 2, 38, 38);
    osBox.setBounds (300, sy + 22, 92, 26);
    bypassButton.setBounds (420, sy + 22, 54, 26);
    meterArea = Rectangle<float> (520, (float) sy + 10, (float) W - 520 - 40, 50);
}

juce::Path VelvetFuzzContent::cablePath() const
{
    Path p;
    const auto& l = order() == 1 ? fuzz : verb;
    const auto& r = order() == 1 ? verb : fuzz;
    const Point<float> a ((float) l.bounds.getRight(), (float) l.bounds.getY() + kJackDY);
    const Point<float> b ((float) r.bounds.getX(), (float) r.bounds.getY() + kJackDY);
    p.startNewSubPath (a);
    p.cubicTo (a.translated (60, 120), b.translated (-60, 120), b);
    return p;
}

// ---------------------------------------------------------------- interaction
void VelvetFuzzContent::mouseDown (const MouseEvent& e)
{
    if (! cableHit.contains (e.position)) return;
    if (auto* prm = proc.apvts.getParameter ("order"))
    {
        // click the cable: Verb > Fuzz -> Fuzz > Verb -> Parallel -> ...
        const int next = (order() + 1) % 3;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) next));
        prm->endChangeGesture();
    }
}

void VelvetFuzzContent::mouseMove (const MouseEvent& e)
{
    const bool h = cableHit.contains (e.position);
    if (h != cableHover) { cableHover = h; setMouseCursor (h ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor); repaint(); }
}

// ---------------------------------------------------------------- painting
void VelvetFuzzContent::drawCable (Graphics& g, Point<float> a, Point<float> b, Colour col, float sag)
{
    Path p;
    p.startNewSubPath (a);
    p.cubicTo (a.translated ((b.x - a.x) * 0.25f, sag), b.translated ((a.x - b.x) * 0.25f, sag), b);
    g.setColour (Colours::black.withAlpha (0.3f));
    g.strokePath (p, PathStrokeType (8.0f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (2, 4));
    g.setColour (col.darker (0.35f));
    g.strokePath (p, PathStrokeType (8.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (col);
    g.strokePath (p, PathStrokeType (4.5f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours::white.withAlpha (0.25f));
    g.strokePath (p, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (-1, -1.5f));
}

void VelvetFuzzContent::drawJack (Graphics& g, Point<float> c)
{
    g.setColour (Colour (0xff1a1a1c));
    g.fillRoundedRectangle (Rectangle<float> (14, 22).withCentre (c), 3.0f);
    g.setGradientFill (ColourGradient (Colour (0xffdedede), c.x - 6, c.y - 6, Colour (0xff6a6a6a), c.x + 6, c.y + 6, false));
    g.fillEllipse (Rectangle<float> (12, 12).withCentre (c));
    g.setColour (Colours::black);
    g.fillEllipse (Rectangle<float> (5, 5).withCentre (c));
}

void VelvetFuzzContent::drawPedal (Graphics& g, const Pedal& p, bool isFuzz)
{
    const auto& t = lnf.theme();
    const auto r = p.bounds.toFloat();
    const bool on = p.foot.getToggleState();
    const bool light = lightPanel (t);

    // enclosure
    const auto base = t.bgBottom.interpolatedWith (isFuzz ? t.accent : t.accent2, 0.22f).withAlpha (1.0f);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r.translated (3, 6), t.corner + 2);
    g.setGradientFill (ColourGradient (base.brighter (0.25f), r.getX(), r.getY(), base.darker (0.35f), r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, t.corner + 2);
    g.setColour (t.panel);
    g.fillRoundedRectangle (r.reduced (6), t.corner);
    g.setColour (t.panelEdge);
    g.drawRoundedRectangle (r.reduced (6.5f), t.corner, 1.0f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (r.reduced (0.5f), t.corner + 2, 1.0f);
    for (auto pt : { r.getTopLeft().translated (13, 13), r.getTopRight().translated (-13, 13), r.getBottomLeft().translated (13, -13), r.getBottomRight().translated (-13, -13) })
    {
        g.setColour (light ? Colour (0xffa08a62) : Colour (0xffb9b9be)); g.fillEllipse (Rectangle<float> (8, 8).withCentre (pt));
        g.setColour (Colours::black.withAlpha (0.5f)); g.drawLine (pt.x - 2.5f, pt.y - 2.5f, pt.x + 2.5f, pt.y + 2.5f, 1.0f);
    }

    // name plate + LED
    g.setColour (on ? t.text : t.textDim);
    g.setFont (titleFont (22.0f));
    g.drawText (p.title, (int) r.getX() + 24, (int) r.getY() + 12, 220, 26, Justification::centredLeft);
    g.setColour (t.textDim);
    g.setFont (Font (FontOptions (12.0f, Font::italic)));
    g.drawText (p.subtitle, (int) r.getX() + 25, (int) r.getY() + 34, 220, 16, Justification::centredLeft);
    const auto led = Rectangle<float> (12, 12).withCentre ({ r.getRight() - 34, r.getY() + 30 });
    const auto ledCol = isFuzz ? Colour (0xffff3b3b) : t.accent2;
    if (on)
    {
        g.setColour (ledCol.withAlpha (0.25f)); g.fillEllipse (led.expanded (7));
        g.setColour (ledCol.withAlpha (0.45f)); g.fillEllipse (led.expanded (3));
    }
    g.setColour (on ? ledCol : ledCol.withMultipliedBrightness (0.3f).withAlpha (1.0f));
    g.fillEllipse (led);
    g.setColour (Colours::black.withAlpha (0.5f)); g.drawEllipse (led, 1.0f);

    if (isFuzz) drawVoltmeter (g, p.display.toFloat());
    else drawSwellDisplay (g, p.display.toFloat());

    // knob labels
    for (auto* k : p.knobs)
    {
        const auto sb = k->slider.getBounds();
        const bool en = k->slider.isEnabled();
        g.setColour ((on && en) ? t.text.withAlpha (0.92f) : t.textDim.withAlpha (0.6f));
        g.setFont (Font (FontOptions (12.0f, Font::bold)));
        g.drawText (k->label.toUpperCase(), sb.getX() - 16, sb.getBottom() + 1, sb.getWidth() + 32, 14, Justification::centred);
        g.setColour (t.textDim.withAlpha (on ? 0.9f : 0.5f));
        g.setFont (Font (FontOptions (11.0f)));
        const String txt = en ? k->slider.getTextFromValue (k->slider.getValue()) : String (proc.getLengthMs()) + " ms sync";
        g.drawText (txt, sb.getX() - 16, sb.getBottom() + 14, sb.getWidth() + 32, 13, Justification::centred);
    }
}

void VelvetFuzzContent::drawSwellDisplay (Graphics& g, Rectangle<float> area)
{
    const auto& t = lnf.theme();
    const auto& vals = proc.apvts;
    const bool tapeMode = vals.getRawParameterValue ("verb_mode")->load() > 0.5f;
    const float swell = vals.getRawParameterValue ("verb_swell")->load();
    const bool on = verb.foot.getToggleState();

    g.setColour (Colours::black.withAlpha (lightPanel (t) ? 0.75f : 0.35f));
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (t.panelEdge);
    g.drawRoundedRectangle (area, 6.0f, 1.0f);
    auto plot = area.reduced (10, 10).withTrimmedBottom (12);

    Path curve, fill;
    const int n = 80;
    const float ex = (float) vf::swellExponent (swell);
    const float k = 6.9f / (2.6f - 2.0f * swell);
    for (int i = 0; i <= n; ++i)
    {
        const float x01 = (float) i / n;
        const float y01 = tapeMode ? std::exp (-k * (1.0f - x01)) : std::pow (x01, ex);
        const Point<float> pt (plot.getX() + x01 * plot.getWidth() * 0.86f, plot.getBottom() - y01 * plot.getHeight());
        if (i == 0) { curve.startNewSubPath (pt); fill.startNewSubPath (plot.getX(), plot.getBottom()); }
        else curve.lineTo (pt);
        fill.lineTo (pt);
    }
    const float cutX = plot.getX() + plot.getWidth() * 0.86f;
    curve.lineTo (cutX, plot.getBottom());   // the gate: it stops dead
    fill.lineTo (cutX, plot.getBottom());
    fill.closeSubPath();
    const auto col = on ? t.accent2 : t.textDim;
    g.setColour (col.withAlpha (0.12f + 0.35f * jlimit (0.0f, 1.0f, wet * 3.0f)));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (curve, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    // the note arrives at the end of the swell
    g.setColour (t.accent);
    g.fillRect (cutX + 6, plot.getY() + 4, 3.0f, plot.getHeight() - 4);

    g.setColour (Colours::white.withAlpha (0.85f));
    g.setFont (titleFont (9.5f));
    const String lat = tapeMode ? "LATENCY " + String (proc.getLengthMs()) + " MS" : "ZERO LATENCY";
    g.drawText ((tapeMode ? "TAPE REVERSE   /   " : "GATED REVERSE   /   ") + lat, area.reduced (10, 4).removeFromBottom (12), Justification::centredLeft);
    g.drawText ("NOTE", Rectangle<float> (cutX - 14, plot.getY() - 9, 50, 10), Justification::centred);
}

void VelvetFuzzContent::drawVoltmeter (Graphics& g, Rectangle<float> area)
{
    const bool on = fuzz.foot.getToggleState();
    // meter window
    auto face = area.withSizeKeepingCentre (jmin (area.getWidth(), 250.0f), area.getHeight());
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (face.expanded (3), 8.0f);
    g.setGradientFill (ColourGradient (Colour (0xfff6ecd6), face.getX(), face.getY(), Colour (0xffd9c7a2), face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 6.0f);

    const Point<float> pivot (face.getCentreX(), face.getBottom() - 14);
    const float rad = face.getHeight() - 34;
    const float a0 = -1.05f, a1 = 1.05f;   // radians from vertical
    auto angleFor = [&] (float v) { return a0 + (a1 - a0) * (jlimit (3.0f, 9.0f, v) - 3.0f) / 6.0f; };

    // red zone (starved supply), scale and labels
    Path red;
    red.addCentredArc (pivot.x, pivot.y, rad, rad, 0.0f, angleFor (3.0f), angleFor (4.5f), true);
    g.setColour (Colour (0xffc8322d));
    g.strokePath (red, PathStrokeType (5.0f));
    Path scale;
    scale.addCentredArc (pivot.x, pivot.y, rad, rad, 0.0f, a0, a1, true);
    g.setColour (Colour (0xff2a2420));
    g.strokePath (scale, PathStrokeType (1.2f));
    g.setFont (Font (FontOptions (10.0f, Font::bold)));
    for (int v = 3; v <= 9; ++v)
    {
        const float a = angleFor ((float) v);
        g.drawLine (Line<float> (pivot.getPointOnCircumference (rad, a), pivot.getPointOnCircumference (rad - (v % 2 ? 8.0f : 5.0f), a)), 1.2f);
        if (v % 2 == 1 || v == 9)
            g.drawText (String (v), Rectangle<float> (20, 12).withCentre (pivot.getPointOnCircumference (rad + 10, a)), Justification::centred);
    }
    g.setFont (titleFont (9.0f));
    g.drawText ("SUPPLY  V", Rectangle<float> (face.getX(), pivot.y - rad * 0.45f, face.getWidth(), 12), Justification::centred);

    // needle
    const float a = angleFor (on ? volts : 9.0f);
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawLine (Line<float> (pivot.translated (1.5f, 1.5f), pivot.getPointOnCircumference (rad - 2, a).translated (1.5f, 1.5f)), 2.0f);
    g.setColour (Colour (0xffb0201c));
    g.drawLine (Line<float> (pivot, pivot.getPointOnCircumference (rad - 2, a)), 1.8f);
    g.setColour (Colour (0xff2a2420));
    g.fillEllipse (Rectangle<float> (10, 10).withCentre (pivot));

    // gate lamp
    const auto lamp = Rectangle<float> (8, 8).withCentre ({ face.getRight() - 16, face.getY() + 14 });
    const bool open = on && gateOpen > 0.5f;
    g.setColour (open ? Colour (0xff3ddc84) : Colour (0xff35503f));
    g.fillEllipse (lamp);
    g.setColour (Colour (0xff2a2420));
    g.setFont (titleFont (8.0f));
    g.drawText ("GATE", Rectangle<float> (40, 10).withCentre ({ face.getRight() - 16, face.getY() + 26 }), Justification::centred);
    g.drawText (String (on ? volts : 9.0f, 1) + " V", Rectangle<float> (60, 12).withCentre ({ face.getX() + 30, face.getY() + 14 }), Justification::centred);
}

void VelvetFuzzContent::drawMeters (Graphics& g, Rectangle<float> area)
{
    const auto& t = lnf.theme();
    auto toMeter = [] (float lin) { return jlimit (0.0f, 1.0f, (float) (Decibels::gainToDecibels (lin, -60.0f) + 60.0f) / 60.0f); };
    auto bar = [&] (Rectangle<float> r, float v, Colour col, const String& name)
    {
        g.setColour (t.textDim);
        g.setFont (titleFont (9.5f));
        g.drawText (name, r.removeFromLeft (34), Justification::centredLeft);
        g.setColour (t.track);
        g.fillRoundedRectangle (r, 3);
        g.setColour (col);
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * v), 3);
    };
    bar (area.removeFromTop (14), toMeter (inMeter), t.accent2, "IN");
    area.removeFromTop (8);
    bar (area.removeFromTop (14), toMeter (outMeter), t.accent, "OUT");
}

void VelvetFuzzContent::paint (Graphics& g)
{
    const auto& t = lnf.theme();
    g.drawImage (background, getLocalBounds().toFloat());

    // title (on light-panel themes the header sits on the dark case, so use the panel colour)
    const bool lpHead = lightPanel (t);
    const auto headText = lpHead ? t.panel : t.text, headDim = lpHead ? t.panel.withAlpha (0.7f) : t.textDim;
    g.setColour (headText);
    g.setFont (titleFont (30.0f));
    g.drawText ("velvet fuzz", 22, 10, 330, 36, Justification::centredLeft);
    g.setColour (headDim);
    g.setFont (titleFont (11.0f));
    g.drawText ("REVERSE REVERB  +  GATED FUZZ", 24, 44, 330, 16, Justification::centredLeft);
    g.setColour (headDim);
    g.setFont (titleFont (9.5f));
    g.drawText ("THEME", 786, 20, 60, 28, Justification::centredRight);

    const int ord = order();
    const auto& left = ord == 1 ? fuzz : verb;
    const auto& right = ord == 1 ? verb : fuzz;
    const float jy = (float) kPedalY + kJackDY;
    const auto cableCol = t.accent2.interpolatedWith (Colours::black, 0.15f);
    const Point<float> lIn ((float) left.bounds.getX(), jy), lOut ((float) left.bounds.getRight(), jy);
    const Point<float> rIn ((float) right.bounds.getX(), jy), rOut ((float) right.bounds.getRight(), jy);

    // cables under the pedals: from the board edges
    drawCable (g, { 0.0f, jy - 10 }, lIn, cableCol, 20);
    drawCable (g, rOut, { (float) W, jy - 10 }, cableCol, 20);

    drawPedal (g, left, &left == &fuzz);
    drawPedal (g, right, &right == &fuzz);
    drawJack (g, lIn); drawJack (g, lOut); drawJack (g, rIn); drawJack (g, rOut);

    // the patch cable (or, in Parallel, a little blend box both outputs plug into)
    const auto hl = cableHover ? t.accent : cableCol;
    const float gx = (float) left.bounds.getRight(), gw = (float) (right.bounds.getX() - left.bounds.getRight());
    if (ord == 2)
    {
        auto box = Rectangle<float> (gw - 50, 70).withCentre ({ gx + gw * 0.5f, jy + 130 });
        drawCable (g, lOut, { box.getX() + 10, box.getY() }, hl, 50);
        drawCable (g, rIn, { box.getRight() - 10, box.getY() }, hl, 50);
        g.setColour (Colour (0xff1c1c20)); g.fillRoundedRectangle (box, 6);
        g.setColour (t.panelEdge); g.drawRoundedRectangle (box, 6, 1);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.setFont (titleFont (10.0f));
        g.drawText ("PARALLEL", box.withTrimmedBottom (box.getHeight() * 0.45f), Justification::centredBottom);
        g.setColour (Colours::white.withAlpha (0.55f));
        g.setFont (Font (FontOptions (10.0f)));
        g.drawText ("both from the dry input", box.withTrimmedTop (box.getHeight() * 0.55f), Justification::centredTop);
    }
    else
        drawCable (g, lOut, rIn, hl, 140);

    g.setColour (headDim);
    g.setFont (titleFont (9.5f));
    g.drawText ("ORDER  (or click the cable)", Rectangle<float> (gx, (float) orderBox.getY() - 18, gw, 14), Justification::centred);
    g.setColour (headText.withAlpha (0.9f));
    g.setFont (titleFont (12.0f));
    const String flow = ord == 0 ? "VERB  >  FUZZ" : ord == 1 ? "FUZZ  >  VERB" : "VERB  +  FUZZ";
    g.drawText (flow, Rectangle<float> (gx, (float) orderBox.getBottom() + 8, gw, 16), Justification::centred);

    // global strip
    const int sy = kPedalY + kPedalH + 18;
    auto strip = Rectangle<float> (24, (float) sy, (float) W - 48, 76);
    g.setColour (t.panel);
    g.fillRoundedRectangle (strip, t.corner);
    g.setColour (t.panelEdge);
    g.drawRoundedRectangle (strip, t.corner, 1.0f);
    const bool lp = lightPanel (t);
    for (auto* k : globalKnobs)
    {
        const auto sb = k->slider.getBounds();
        g.setColour (lp ? t.text : t.text.withAlpha (0.92f));
        g.setFont (Font (FontOptions (11.0f, Font::bold)));
        g.drawText (k->label.toUpperCase(), sb.getX() - 20, sb.getBottom(), sb.getWidth() + 40, 12, Justification::centred);
        g.setColour (t.textDim);
        g.setFont (Font (FontOptions (10.0f)));
        g.drawText (k->slider.getTextFromValue (k->slider.getValue()), sb.getX() - 20, sb.getBottom() + 11, sb.getWidth() + 40, 11, Justification::centred);
    }
    g.setColour (t.textDim);
    g.setFont (titleFont (9.0f));
    g.drawText ("OVERSAMPLING", osBox.getBounds().translated (0, -16).withHeight (14).expanded (10, 0), Justification::centred);
    g.drawText ("BYPASS", bypassButton.getBounds().translated (0, -16).withHeight (14).expanded (10, 0), Justification::centred);
    drawMeters (g, meterArea);
}

// ---------------------------------------------------------------- presets
void VelvetFuzzContent::showPresetMenu()
{
    PopupMenu menu;
    const auto& ps = vf::factoryPresets();
    for (const auto& bank : vf::presetBanks())
    {
        PopupMenu sub;
        for (size_t i = 0; i < ps.size(); ++i)
            if (ps[i].bank == bank) sub.addItem ((int) i + 1, ps[i].name, true, (int) i == proc.getCurrentProgram() && proc.getPresetName() == String (ps[i].name));
        menu.addSubMenu (bank, sub);
    }
    const auto users = proc.userPresets();
    PopupMenu userMenu;
    for (int i = 0; i < users.size(); ++i) userMenu.addItem (10000 + i, users[i].getFileNameWithoutExtension());
    if (users.isEmpty()) userMenu.addItem (-1, "(none saved yet)", false);
    menu.addSubMenu ("User", userMenu);
    menu.addSeparator();
    menu.addItem (20000, "Save current as...");
    menu.addItem (20001, "Show preset folder");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (presetButton.getWidth()),
                        [this, users] (int r)
                        {
                            if (r <= 0) return;
                            if (r < 10000) proc.loadFactoryPreset (r - 1);
                            else if (r < 20000) proc.loadUserPreset (users[r - 10000]);
                            else if (r == 20000) savePresetDialog();
                            else if (r == 20001)
                            {
                                VelvetFuzzProcessor::userPresetFolder().createDirectory();
                                VelvetFuzzProcessor::userPresetFolder().revealToUser();
                            }
                        });
}

void VelvetFuzzContent::savePresetDialog()
{
    saveWindow = std::make_unique<AlertWindow> ("Save preset", "Name for this preset:", MessageBoxIconType::NoIcon, this);
    saveWindow->setLookAndFeel (&lnf);
    saveWindow->addTextEditor ("name", proc.getPresetName());
    saveWindow->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    saveWindow->enterModalState (true, ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveWindow != nullptr) proc.saveUserPreset (saveWindow->getTextEditorContents ("name"));
        saveWindow.reset();
    }), false);
}

// ---------------------------------------------------------------- animation and live labels
void VelvetFuzzContent::timerCallback()
{
    const auto& e = proc.getEngine();
    inMeter = e.inLevel.load();
    outMeter = e.outLevel.load();
    wet = wet * 0.8f + 0.2f * e.wetLevel.load();
    gateOpen = e.gateLevel.load();
    volts += (e.supplyVolts.load() - volts) * 0.35f;   // needle ballistics

    if (order() != shownOrder) resized();

    if (shownPreset != proc.getPresetName())
    {
        shownPreset = proc.getPresetName();
        const int idx = proc.getCurrentProgram();
        const auto& ps = vf::factoryPresets();
        String bank = "User";
        if (isPositiveAndBelow (idx, (int) ps.size()) && String (ps[(size_t) idx].name) == shownPreset) bank = ps[(size_t) idx].bank;
        presetButton.setButtonText (shownPreset + String (CharPointer_UTF8 ("   \xc2\xb7   ")) + bank);
    }
    if (lengthKnob != nullptr)
        lengthKnob->slider.setEnabled (proc.apvts.getRawParameterValue ("verb_sync")->load() < 0.5f);
    for (auto* pd : { &verb, &fuzz })
    {
        const bool on = pd->foot.getToggleState();
        for (auto* k : pd->knobs) k->slider.setAlpha (on ? 1.0f : 0.45f);
    }
    modeBox.setAlpha (verb.foot.getToggleState() ? 1.0f : 0.5f);
    syncBox.setAlpha (verb.foot.getToggleState() ? 1.0f : 0.5f);
    repaint();
}

// ============================================================================ editor
VelvetFuzzEditor::VelvetFuzzEditor (VelvetFuzzProcessor& p) : AudioProcessorEditor (p), proc (p), content (p)
{
    addAndMakeVisible (content);
    const double ratio = (double) VelvetFuzzContent::W / VelvetFuzzContent::H;
    setResizable (true, true);
    setResizeLimits ((int) (VelvetFuzzContent::W * 0.7), (int) (VelvetFuzzContent::H * 0.7), VelvetFuzzContent::W * 2, VelvetFuzzContent::H * 2);
    getConstrainer()->setFixedAspectRatio (ratio);
    const float s = jlimit (0.7f, 2.0f, proc.uiScale);
    setSize (roundToInt (VelvetFuzzContent::W * s), roundToInt (VelvetFuzzContent::H * s));
}

void VelvetFuzzEditor::resized()
{
    // fit inside the editor even when a host ignores the aspect ratio
    const float s = jmin ((float) getWidth() / (float) VelvetFuzzContent::W, (float) getHeight() / (float) VelvetFuzzContent::H);
    content.setTransform (AffineTransform::scale (s).translated ((getWidth() - VelvetFuzzContent::W * s) * 0.5f, 0.0f));
    content.setBounds (0, 0, VelvetFuzzContent::W, VelvetFuzzContent::H);
    proc.uiScale = s;
}
