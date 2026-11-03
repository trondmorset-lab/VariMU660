#include "PluginEditor.h"

namespace
{
    const juce::Colour panelTop    (0xffbdbab1);
    const juce::Colour panelBottom (0xff98958c);
    const juce::Colour ink         (0xff1d1c1a);
    const juce::Colour engraving   (0xff3b3935);
    const juce::Colour vuCream     (0xfff3e6c4);
    const juce::Colour vuRed       (0xffb3261e);

    constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;
    constexpr float kEndAngle   = juce::MathConstants<float>::pi * 2.75f;

    juce::Font engravedFont (float size, bool bold = true)
    {
       #if JUCE_MAC || JUCE_WINDOWS
        const juce::String face ("Georgia");
       #else
        const juce::String face (juce::Font::getDefaultSerifFontName());
       #endif
        return juce::Font (juce::FontOptions (face, size, bold ? juce::Font::bold : juce::Font::plain));
    }

    void drawScrew (juce::Graphics& g, juce::Point<float> c)
    {
        const float r = 7.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe0ded8), c.x - r, c.y - r,
                                                 juce::Colour (0xff6d6b66), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (juce::Colour (0xff4a4844));
        g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
        g.drawLine (c.x - r * 0.7f, c.y - r * 0.2f, c.x + r * 0.7f, c.y + r * 0.2f, 1.6f);
    }
}

//==============================================================================
void VintageLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto knob = bounds.withSizeKeepingCentre (size, size);
    const auto c = knob.getCentre();
    const float r = size * 0.5f;

    // skygge
    g.setColour (juce::Colour (0x55000000));
    g.fillEllipse (knob.translated (2.5f, 4.0f));

    // skjørt (bredere nedre del)
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3835), c.x, knob.getY(),
                                             juce::Colour (0xff0b0b0a), c.x, knob.getBottom(), false));
    g.fillEllipse (knob);

    // riller rundt kanten
    g.setColour (juce::Colour (0xff050505));
    for (int i = 0; i < 36; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine (c.x + std::sin (a) * r * 0.86f, c.y - std::cos (a) * r * 0.86f,
                    c.x + std::sin (a) * r * 0.99f, c.y - std::cos (a) * r * 0.99f, 1.2f);
    }

    // topp med glans
    auto top = knob.reduced (r * 0.2f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b4945), top.getX(), top.getY(),
                                             juce::Colour (0xff111110), top.getRight(), top.getBottom(), false));
    g.fillEllipse (top);
    g.setColour (juce::Colour (0x22ffffff));
    g.fillEllipse (top.reduced (top.getWidth() * 0.18f).translated (-r * 0.12f, -r * 0.14f));

    // hvit peker
    const float angle = startAngle + pos * (endAngle - startAngle);
    juce::Path pointer;
    pointer.addRoundedRectangle (-r * 0.045f, -r * 0.95f, r * 0.09f, r * 0.5f, r * 0.03f);
    g.setColour (juce::Colour (0xfff2efe6));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void VintageLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto area = b.getLocalBounds().toFloat();
    auto lamp = area.removeFromLeft (area.getHeight()).reduced (5.0f);

    // jewel-lampe
    const bool on = b.getToggleState();
    g.setColour (juce::Colour (0xff2a2826));
    g.fillEllipse (lamp.expanded (2.0f));
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffffd27a) : juce::Colour (0xff5a4a2c),
                                             lamp.getCentreX(), lamp.getY(),
                                             on ? juce::Colour (0xffd2741a) : juce::Colour (0xff2c2418),
                                             lamp.getCentreX(), lamp.getBottom(), false));
    g.fillEllipse (lamp);
    if (on)
    {
        g.setColour (juce::Colour (0x30ffb040));
        g.fillEllipse (lamp.expanded (5.0f));
    }

    g.setColour (ink);
    g.setFont (engravedFont (13.0f));
    g.drawText (b.getButtonText(), area.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
}

//==============================================================================
GainReductionMeter::GainReductionMeter (std::atomic<float>& source) : grSource (source)
{
    startTimerHz (60);
}

void GainReductionMeter::timerCallback()
{
    // VU-ballistikk (~300 ms). Tegn bare på nytt når nålen faktisk flytter seg.
    const float target = juce::jlimit (0.0f, 22.0f, grSource.load());
    const float next = needleDb + (target - needleDb) * 0.075f;
    if (std::abs (next - needleDb) > 0.005f)
    {
        needleDb = next;
        repaint();
    }
}

// Plassering på buen (0–1). VU-skalaen er lineær i amplitude, med 0 dB ved 72 %.
float GainReductionMeter::dbToPosition (float grDb)
{
    return juce::jlimit (0.0f, 1.0f, juce::Decibels::decibelsToGain (-grDb) * 0.72f);
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // svart ramme
    g.setColour (juce::Colour (0xff141312));
    g.fillRoundedRectangle (b, 8.0f);
    auto face = b.reduced (9.0f);

    g.setGradientFill (juce::ColourGradient (vuCream.brighter (0.15f), face.getCentreX(), face.getY(),
                                             vuCream.darker (0.12f), face.getCentreX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 4.0f);

    g.saveState();
    g.reduceClipRegion (face.toNearestInt());

    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.32f);
    const float arcR = (pivot.y - face.getY()) - 34.0f;
    const float a0 = -0.72f, a1 = 0.72f; // vinkelområde (rad, 0 = rett opp)

    auto angleFor = [&] (float p) { return a0 + p * (a1 - a0); };
    auto pointAt  = [&] (float angle, float rr) { return juce::Point<float> (pivot.x + std::sin (angle) * rr, pivot.y - std::cos (angle) * rr); };

    // bue: svart fra -20 til 0, rød over 0
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, arcR, arcR, 0.0f, a0, angleFor (0.72f), true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.8f));
    juce::Path redArc;
    redArc.addCentredArc (pivot.x, pivot.y, arcR + 2.5f, arcR + 2.5f, 0.0f, angleFor (0.72f), a1, true);
    g.setColour (vuRed);
    g.strokePath (redArc, juce::PathStrokeType (5.0f));

    // skalamerker (GR i dB)
    const float marks[] = { 0.0f, 1.0f, 2.0f, 3.0f, 5.0f, 7.0f, 10.0f, 20.0f };
    for (float m : marks)
    {
        const float a = angleFor (dbToPosition (m));
        g.setColour (ink);
        g.drawLine (juce::Line<float> (pointAt (a, arcR), pointAt (a, arcR + 9.0f)), 1.5f);
        const auto tp = pointAt (a, arcR + 20.0f);
        g.setFont (engravedFont (m < 0.5f ? 13.0f : 11.0f));
        g.drawText (m < 0.5f ? "0" : "-" + juce::String ((int) m),
                    juce::Rectangle<float> (26.0f, 14.0f).withCentre (tp), juce::Justification::centred);
    }
    // små mellommerker
    for (float m : { 0.5f, 1.5f, 2.5f, 4.0f, 6.0f, 8.5f, 15.0f })
    {
        const float a = angleFor (dbToPosition (m));
        g.drawLine (juce::Line<float> (pointAt (a, arcR), pointAt (a, arcR + 5.0f)), 1.0f);
    }

    g.setFont (engravedFont (11.0f, false));
    g.setColour (engraving);
    g.drawText ("GAIN REDUCTION", face.withTrimmedTop (face.getHeight() * 0.56f).removeFromTop (14.0f)
                                        .withTrimmedLeft (face.getWidth() * 0.5f + 10.0f).withTrimmedRight (8.0f),
                juce::Justification::centredLeft);
    g.setFont (engravedFont (18.0f));
    g.setColour (ink);
    g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.56f).removeFromTop (20.0f)
                          .withTrimmedRight (face.getWidth() * 0.5f + 16.0f),
                juce::Justification::centredRight);

    // nål
    const float na = angleFor (dbToPosition (needleDb));
    g.setColour (juce::Colour (0x40000000));
    g.drawLine (juce::Line<float> (pivot.translated (2, 2), pointAt (na, arcR + 12.0f).translated (2, 2)), 2.0f);
    g.setColour (ink);
    g.drawLine (juce::Line<float> (pivot, pointAt (na, arcR + 12.0f)), 1.6f);
    g.restoreState();

    // glass-refleks
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x26ffffff), face.getX(), face.getY(),
                                             juce::Colour (0x00ffffff), face.getX(), face.getCentreY(), false));
    g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.5f), 4.0f);

    // numerisk lesing
    g.setFont (engravedFont (11.0f, false));
    g.setColour (engraving);
    g.drawText (juce::String (-needleDb, 1) + " dB", face.reduced (6.0f), juce::Justification::bottomRight);
}

//==============================================================================
VariMuAudioProcessorEditor::VariMuAudioProcessorEditor (VariMuAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), meter (p.gainReductionDb)
{
    setLookAndFeel (&lnf);

    setupKnob (input,     ParamIDs::input,     "INPUT GAIN");
    setupKnob (threshold, ParamIDs::threshold, "THRESHOLD");
    setupKnob (timeConst, ParamIDs::timeConst, "TIME CONSTANT");
    setupKnob (output,    ParamIDs::output,    "OUTPUT");
    setupKnob (color,     ParamIDs::color,     "TUBE COLOR");
    setupKnob (mix,       ParamIDs::mix,       "MIX");

    // Time Constant er en trinnbryter: vis "POS 1" til "POS 6"
    timeConst.slider.textFromValueFunction = [] (double v) { return "POS " + juce::String ((int) std::round (v) + 1); };
    timeConst.slider.updateText();

    addAndMakeVisible (linkButton);
    linkAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, ParamIDs::link, linkButton);

    addAndMakeVisible (meter);
    setSize (940, 420);
}

VariMuAudioProcessorEditor::~VariMuAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void VariMuAudioProcessorEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& label)
{
    k.label = label;
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (kStartAngle, kEndAngle, true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 18);
    k.slider.setColour (juce::Slider::textBoxTextColourId, ink);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x14000000));
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x30000000));
    k.slider.setColour (juce::Slider::textBoxHighlightColourId, juce::Colour (0x40000000));
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, paramId, k.slider);

    // Dobbeltklikk setter knotten tilbake til standardverdien
    auto* param = audioProcessor.apvts.getParameter (paramId);
    k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void VariMuAudioProcessorEditor::drawKnobScale (juce::Graphics& g, const Knob& k, int numTicks,
                                                const juce::StringArray& labels)
{
    auto sb = k.slider.getBounds().toFloat();
    auto knobArea = sb.withTrimmedBottom (18.0f);   // under knotten ligger tekstfeltet
    const float size = juce::jmin (knobArea.getWidth(), knobArea.getHeight()) - 8.0f;
    const auto c = knobArea.getCentre();
    const float r = size * 0.5f;

    g.setColour (engraving);
    for (int i = 0; i < numTicks; ++i)
    {
        const float t = numTicks > 1 ? (float) i / (float) (numTicks - 1) : 0.0f;
        const float a = kStartAngle + t * (kEndAngle - kStartAngle);
        g.drawLine (c.x + std::sin (a) * (r + 4), c.y - std::cos (a) * (r + 4),
                    c.x + std::sin (a) * (r + 10), c.y - std::cos (a) * (r + 10), 1.5f);
        if (i < labels.size() && labels[i].isNotEmpty())
        {
            g.setFont (engravedFont (10.5f));
            const juce::Point<float> tp (c.x + std::sin (a) * (r + 21), c.y - std::cos (a) * (r + 21));
            g.drawText (labels[i], juce::Rectangle<float> (30.0f, 14.0f).withCentre (tp), juce::Justification::centred);
        }
    }

    g.setColour (ink);
    g.setFont (engravedFont (13.0f));
    g.drawText (k.label, juce::Rectangle<float> (sb.getX() - 40, knobArea.getCentreY() - r - 50, sb.getWidth() + 80, 16),
                juce::Justification::centred);
}

void VariMuAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // børstet stålpanel
    g.setGradientFill (juce::ColourGradient (panelTop, 0, 0, panelBottom, 0, b.getHeight(), false));
    g.fillAll();
    juce::Random rng (66);
    for (int yy = 0; yy < getHeight(); yy += 2)
    {
        g.setColour (juce::Colour::fromFloatRGBA (1, 1, 1, rng.nextFloat() * 0.05f));
        g.drawHorizontalLine (yy, 0.0f, b.getWidth());
    }

    // rackøre-skruer
    drawScrew (g, { 22.0f, 22.0f });
    drawScrew (g, { b.getWidth() - 22.0f, 22.0f });
    drawScrew (g, { 22.0f, b.getHeight() - 22.0f });
    drawScrew (g, { b.getWidth() - 22.0f, b.getHeight() - 22.0f });

    // tittel
    g.setColour (ink);
    g.setFont (engravedFont (24.0f));
    g.drawText ("VARI-MU 660", juce::Rectangle<float> (0, 14, b.getWidth(), 28), juce::Justification::centred);
    g.setFont (engravedFont (11.0f, false));
    g.setColour (engraving);
    g.drawText ("VARIABLE-MU  TUBE  COMPRESSOR  /  LIMITER", juce::Rectangle<float> (0, 40, b.getWidth(), 16),
                juce::Justification::centred);

    // innfelt linje under tittel
    g.setColour (juce::Colour (0x40000000));
    g.drawHorizontalLine (62, 50.0f, b.getWidth() - 50.0f);
    g.setColour (juce::Colour (0x50ffffff));
    g.drawHorizontalLine (63, 50.0f, b.getWidth() - 50.0f);

    drawKnobScale (g, input,     11, { "-12", "", "", "", "", "", "", "", "", "", "+24" });
    drawKnobScale (g, threshold, 11, { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" });
    drawKnobScale (g, timeConst, 6,  { "1", "2", "3", "4", "5", "6" });
    drawKnobScale (g, output,    11, { "-24", "", "", "", "", "0", "", "", "", "", "+24" });
    drawKnobScale (g, color,     5,  {});
    drawKnobScale (g, mix,       5,  {});
}

void VariMuAudioProcessorEditor::resized()
{
    const int big = 104;           // knottdiameter-område
    const int boxH = 18;
    const int rowY = 128;          // øvre kant av store knotter
    auto place = [&] (Knob& k, int cx, int size, int y)
    {
        k.slider.setBounds (cx - (size + 16) / 2, y, size + 16, size + boxH);
    };

    const int w = getWidth();
    place (input,     110,     big, rowY);
    place (threshold, 252,     big, rowY);
    place (timeConst, w - 252, big, rowY);
    place (output,    w - 110, big, rowY);

    meter.setBounds (juce::Rectangle<int> (232, 150).withCentre ({ w / 2, rowY + big / 2 + 4 }));

    // nedre rad
    const int small = 56;
    const int lowY = 318;
    place (color, w / 2 - 150, small, lowY);
    place (mix,   w / 2 + 150, small, lowY);
    linkButton.setBounds (juce::Rectangle<int> (150, 26).withCentre ({ w / 2 + 6, lowY + small / 2 }));
}
