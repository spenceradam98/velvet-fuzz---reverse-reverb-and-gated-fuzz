#include "Themes.h"

using namespace juce;

const std::vector<Theme>& themes()
{
    static const std::vector<Theme> list = []
    {
        std::vector<Theme> v;
        Theme t;

        // Pink Bleach: shoegaze sleeve, sun-bleached pink, cream knobs, film grain
        t = {};
        t.name = "Pink Bleach";
        t.bgTop = Colour (0xffe7a3b6); t.bgBottom = Colour (0xff6a2f4b);
        t.panel = Colour (0x1fffffff); t.panelEdge = Colour (0x55fff1f5);
        t.text = Colour (0xfffff6f8); t.textDim = Colour (0xccffdfe9);
        t.accent = Colour (0xffff5fa2); t.accent2 = Colour (0xfffff0b8);
        t.knobFace = Colour (0xfffff5ef); t.knobRim = Colour (0xffe2b7c4); t.track = Colour (0x40ffffff);
        t.knobStyle = 0; t.glow = true; t.grain = true; t.grainAmount = 0.08f; t.corner = 12.0f;
        v.push_back (t);

        // Dusk: pastel twilight gradient, frosted glass panels, soft glow
        t = {};
        t.name = "Dusk";
        t.bgTop = Colour (0xff2d2350); t.bgBottom = Colour (0xffc9839f);
        t.panel = Colour (0x26ffffff); t.panelEdge = Colour (0x40ffffff);
        t.text = Colour (0xfffff4f8); t.textDim = Colour (0xbbfbe3ee);
        t.accent = Colour (0xffffb8d9); t.accent2 = Colour (0xffaedcff);
        t.knobFace = Colour (0xfff7eaf2); t.knobRim = Colour (0xffd8c2d8); t.track = Colour (0x40ffffff);
        t.knobStyle = 0; t.glow = true; t.stars = true; t.corner = 14.0f;
        v.push_back (t);

        // Neon: synth pop night, black glass, magenta / cyan rings, scanlines
        t = {};
        t.name = "Neon";
        t.bgTop = Colour (0xff07040f); t.bgBottom = Colour (0xff1e0a33);
        t.panel = Colour (0xcc0f0a1c); t.panelEdge = Colour (0xffff3fb4).withAlpha (0.45f);
        t.text = Colour (0xfff3eaff); t.textDim = Colour (0xff9f8cc0);
        t.accent = Colour (0xffff3fb4); t.accent2 = Colour (0xff35f0ff);
        t.knobFace = Colour (0xff151021); t.knobRim = Colour (0xff2b2240); t.track = Colour (0xff2a2140);
        t.knobStyle = 1; t.glow = true; t.scanlines = true; t.corner = 4.0f;
        v.push_back (t);

        // Fuzz: shoegaze, washed teal and sand, heavy film grain
        t = {};
        t.name = "Fuzz";
        t.bgTop = Colour (0xff1d2b30); t.bgBottom = Colour (0xff5d7774);
        t.panel = Colour (0x8c22353a); t.panelEdge = Colour (0x30f2d0a4);
        t.text = Colour (0xfff4e6d0); t.textDim = Colour (0xffb3c2b8);
        t.accent = Colour (0xfff2c58e); t.accent2 = Colour (0xff9fd4c9);
        t.knobFace = Colour (0xff35494c); t.knobRim = Colour (0xff22302f); t.track = Colour (0x40f4e6d0);
        t.knobStyle = 2; t.grain = true; t.grainAmount = 0.10f; t.corner = 8.0f;
        v.push_back (t);

        // Velvet: vintage console, burgundy case, cream panels, bakelite knobs
        t = {};
        t.name = "Velvet";
        t.bgTop = Colour (0xff4a1d22); t.bgBottom = Colour (0xff2a0f12);
        t.panel = Colour (0xfff0e4c8); t.panelEdge = Colour (0xffb59a6a);
        t.text = Colour (0xff3a2418); t.textDim = Colour (0xff7a604a);
        t.accent = Colour (0xffb8432f); t.accent2 = Colour (0xff3f6b5a);
        t.knobFace = Colour (0xff1c1614); t.knobRim = Colour (0xffd9c9a3); t.track = Colour (0x40000000);
        t.knobStyle = 3; t.grain = true; t.grainAmount = 0.04f; t.corner = 6.0f;
        v.push_back (t);

        // Mono: minimal, high contrast
        t = {};
        t.name = "Mono";
        t.bgTop = Colour (0xff121212); t.bgBottom = Colour (0xff0b0b0b);
        t.panel = Colour (0xff1b1b1b); t.panelEdge = Colour (0xff2e2e2e);
        t.text = Colour (0xfff2f2f2); t.textDim = Colour (0xff8a8a8a);
        t.accent = Colour (0xfff2f2f2); t.accent2 = Colour (0xff9a9a9a);
        t.knobFace = Colour (0xff262626); t.knobRim = Colour (0xff3a3a3a); t.track = Colour (0xff333333);
        t.knobStyle = 2; t.corner = 2.0f;
        v.push_back (t);
        return v;
    }();
    return list;
}

VelvetLookAndFeel::VelvetLookAndFeel() { setTheme (themes().front()); }

void VelvetLookAndFeel::setTheme (const Theme& th)
{
    t = th;
    const bool lightPanel = (t.panel.getPerceivedBrightness() > 0.6f && t.panel.getFloatAlpha() > 0.8f);
    const auto popupBg = lightPanel ? Colour (0xfff6eedb) : t.bgTop.darker (0.3f).withAlpha (1.0f);
    setColour (ComboBox::backgroundColourId, Colours::transparentBlack);
    setColour (ComboBox::textColourId, t.text);
    setColour (ComboBox::outlineColourId, t.panelEdge);
    setColour (ComboBox::arrowColourId, t.textDim);
    setColour (PopupMenu::backgroundColourId, popupBg);
    setColour (PopupMenu::textColourId, lightPanel ? Colour (0xff3a2418) : t.text);
    setColour (PopupMenu::headerTextColourId, t.accent);
    setColour (PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.35f));
    setColour (PopupMenu::highlightedTextColourId, lightPanel ? Colours::black : Colours::white);
    setColour (TextButton::textColourOffId, t.text);
    setColour (TextButton::textColourOnId, t.text);
    setColour (TextButton::buttonColourId, Colours::transparentBlack);
    setColour (Label::textColourId, t.text);
    setColour (TextEditor::backgroundColourId, popupBg);
    setColour (TextEditor::textColourId, lightPanel ? Colour (0xff3a2418) : t.text);
    setColour (TextEditor::outlineColourId, t.panelEdge);
    setColour (AlertWindow::backgroundColourId, popupBg);
    setColour (AlertWindow::textColourId, lightPanel ? Colour (0xff3a2418) : t.text);
    setColour (TooltipWindow::backgroundColourId, popupBg);
    setColour (TooltipWindow::textColourId, lightPanel ? Colour (0xff3a2418) : t.text);
}

void VelvetLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float d = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float r = d * 0.5f;
    const float angle = start + pos * (end - start);
    const bool bipolar = s.getProperties()["bipolar"];
    const float from = bipolar ? (start + end) * 0.5f : start;
    const bool enabled = s.isEnabled();
    const auto acc = enabled ? t.accent : t.textDim;

    auto arc = [&] (float rad, float a0, float a1, Colour col, float thick)
    {
        Path p;
        p.addCentredArc (c.x, c.y, rad, rad, 0.0f, jmin (a0, a1), jmax (a0, a1), true);
        g.setColour (col);
        g.strokePath (p, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
    };

    switch (t.knobStyle)
    {
        case 1:   // neon ring
        {
            arc (r - 3, start, end, t.track, 4.0f);
            if (t.glow && enabled)
                for (int i = 3; i >= 1; --i) arc (r - 3, from, angle, acc.withAlpha (0.12f), 4.0f + i * 4.0f);
            arc (r - 3, from, angle, acc, 3.0f);
            auto face = Rectangle<float> (d * 0.62f, d * 0.62f).withCentre (c);
            g.setColour (t.knobFace); g.fillEllipse (face);
            g.setColour (t.accent2.withAlpha (0.5f)); g.drawEllipse (face, 1.2f);
            g.setColour (t.accent2);
            g.drawLine (Line<float> (c.getPointOnCircumference (d * 0.12f, angle), c.getPointOnCircumference (d * 0.29f, angle)), 2.2f);
            break;
        }
        case 2:   // flat matte
        {
            arc (r - 3, start, end, t.track, 3.0f);
            arc (r - 3, from, angle, acc, 3.0f);
            auto face = Rectangle<float> (d * 0.68f, d * 0.68f).withCentre (c);
            g.setColour (t.knobFace); g.fillEllipse (face);
            g.setColour (t.knobRim); g.drawEllipse (face, 1.0f);
            g.setColour (t.text);
            g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (c.getPointOnCircumference (d * 0.24f, angle)));
            break;
        }
        case 3:   // bakelite pointer knob with cream skirt and printed scale
        {
            for (int i = 0; i <= 10; ++i)
            {
                const float a = start + (end - start) * i / 10.0f;
                g.setColour (t.text.withAlpha (i % 5 == 0 ? 0.8f : 0.45f));
                g.drawLine (Line<float> (c.getPointOnCircumference (r - 1, a), c.getPointOnCircumference (r - (i % 5 == 0 ? 6.0f : 4.0f), a)), 1.2f);
            }
            auto skirt = Rectangle<float> (d * 0.78f, d * 0.78f).withCentre (c);
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillEllipse (skirt.translated (1.5f, 2.5f));
            g.setGradientFill (ColourGradient (Colour (0xfff7efd9), skirt.getX(), skirt.getY(), Colour (0xffb9a679), skirt.getRight(), skirt.getBottom(), false));
            g.fillEllipse (skirt);
            auto cap = skirt.reduced (d * 0.12f);
            g.setGradientFill (ColourGradient (Colour (0xff4a3a32), cap.getX(), cap.getY(), Colour (0xff0d0908), cap.getRight(), cap.getBottom(), false));
            g.fillEllipse (cap);
            Path ptr;
            ptr.addTriangle (-d * 0.06f, -d * 0.12f, d * 0.06f, -d * 0.12f, 0.0f, -d * 0.39f);
            g.setColour (Colour (0xff15100e));
            g.fillPath (ptr, AffineTransform::rotation (angle).translated (c));
            g.setColour (t.accent);
            g.drawLine (Line<float> (c.getPointOnCircumference (d * 0.24f, angle), c.getPointOnCircumference (d * 0.37f, angle)), 2.0f);
            break;
        }
        default:  // soft glass with glowing arc
        {
            arc (r - 3, start, end, t.track, 3.5f);
            if (t.glow && enabled) arc (r - 3, from, angle, acc.withAlpha (0.25f), 9.0f);
            arc (r - 3, from, angle, acc, 3.5f);
            auto face = Rectangle<float> (d * 0.66f, d * 0.66f).withCentre (c);
            g.setColour (Colours::black.withAlpha (0.18f)); g.fillEllipse (face.translated (0, 2.5f));
            g.setGradientFill (ColourGradient (t.knobFace, face.getX(), face.getY(), t.knobRim, face.getRight(), face.getBottom(), false));
            g.fillEllipse (face);
            g.setColour (Colours::white.withAlpha (0.5f)); g.drawEllipse (face.reduced (1), 1.0f);
            g.setColour (t.bgTop);
            g.drawLine (Line<float> (c.getPointOnCircumference (d * 0.10f, angle), c.getPointOnCircumference (d * 0.28f, angle)), 2.5f);
            break;
        }
    }
}

void VelvetLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    // power switch: a small pill with an LED dot
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const float h = r.getHeight();
    auto pill = r.withWidth (h * 1.8f).withCentre (r.getCentre());
    g.setColour (on ? t.accent.withAlpha (0.35f) : t.track);
    g.fillRoundedRectangle (pill, h * 0.5f);
    g.setColour (t.panelEdge.withAlpha (highlighted ? 1.0f : 0.7f));
    g.drawRoundedRectangle (pill, h * 0.5f, 1.0f);
    auto dot = Rectangle<float> (h - 4, h - 4).withCentre ({ on ? pill.getRight() - h * 0.5f : pill.getX() + h * 0.5f, pill.getCentreY() });
    if (on && t.glow) { g.setColour (t.accent.withAlpha (0.3f)); g.fillEllipse (dot.expanded (3)); }
    g.setColour (on ? t.accent : t.textDim);
    g.fillEllipse (dot);
}

void VelvetLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (t.panel.withMultipliedAlpha (down ? 2.0f : (highlighted ? 1.6f : 1.0f)));
    g.fillRoundedRectangle (r, jmin (t.corner, r.getHeight() * 0.5f));
    g.setColour (highlighted ? t.accent.withAlpha (0.8f) : t.panelEdge);
    g.drawRoundedRectangle (r, jmin (t.corner, r.getHeight() * 0.5f), 1.0f);
}

void VelvetLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (t.panel.withMultipliedAlpha (box.isMouseOver() ? 1.6f : 1.0f));
    g.fillRoundedRectangle (r, jmin (t.corner * 0.6f, r.getHeight() * 0.5f));
    g.setColour (t.panelEdge);
    g.drawRoundedRectangle (r, jmin (t.corner * 0.6f, r.getHeight() * 0.5f), 1.0f);
    Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (t.textDim);
    g.fillPath (arrow);
}

Font VelvetLookAndFeel::getComboBoxFont (ComboBox&) { return Font (FontOptions (13.0f)); }
Font VelvetLookAndFeel::getTextButtonFont (TextButton&, int h) { return Font (FontOptions (jmin (15.0f, (float) h * 0.6f), Font::bold)); }
Font VelvetLookAndFeel::getPopupMenuFont() { return Font (FontOptions (14.0f)); }
Font VelvetLookAndFeel::getSliderPopupFont (Slider&) { return Font (FontOptions (13.0f, Font::bold)); }

void VelvetLookAndFeel::drawBubble (Graphics& g, BubbleComponent&, const Point<float>&, const Rectangle<float>& body)
{
    g.setColour (findColour (TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle (body, 5.0f);
    g.setColour (t.accent);
    g.drawRoundedRectangle (body, 5.0f, 1.0f);
}
