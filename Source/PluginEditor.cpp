#include <BinaryData.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include "PluginEditor.h"

namespace
{
constexpr int kW = 1120;
constexpr int kH = 648;

// colours sampled from the DMF LAB logo
const juce::Colour kBg        (0xff000000);
const juce::Colour kPanelLo   (0xff07040b);
const juce::Colour kPanel     (0xff0c0812);
const juce::Colour kPanelHi   (0xff181022);
const juce::Colour kPurple    (0xff472665);
const juce::Colour kPurpleDim (0xff2a1545);
const juce::Colour kPurpleGlow(0xff9155ea);
const juce::Colour kLime      (0xff7ef326);
const juce::Colour kLimeLight (0xffe4ffc4);
const juce::Colour kText      (0xffeef9e4);
const juce::Colour kMuted     (0xff8f80a8);
const juce::Colour kGrid      (0xff1b1229);

const char* const kKnobIds[6]   = { "speed", "stab", "range", "formant", "chorus", "mix" };
const char* const kKnobNames[6] = { "SPEED", "STABILIZE", "RANGE", "FORMANT", "CHORUS", "MIX" };
const char* const kNoteIds[12]  = { "n0", "n1", "n2", "n3", "n4", "n5", "n6", "n7", "n8", "n9", "n10", "n11" };

// main pitch display
constexpr int kPlotX = 84, kPlotY = 150, kPlotW = 680, kLanes = 24, kLaneH = 12;
constexpr int kBlackKeys = 0b010101001010;
constexpr float kWinSeconds = 8.0f;

juce::Font monoFont (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font titleFont (float h)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), h, juce::Font::bold));
}

void panel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (kPanel);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (kPurple);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
}

void glowLine (juce::Graphics& g, float x1, float y1, float x2, float y2, juce::Colour c)
{
    g.setColour (c.withAlpha (0.16f));
    g.drawLine (x1, y1, x2, y2, 7.0f);
    g.setColour (c.withAlpha (0.45f));
    g.drawLine (x1, y1, x2, y2, 2.5f);
    g.setColour (c);
    g.drawLine (x1, y1, x2, y2, 1.0f);
}

void sectionLabel (juce::Graphics& g, int x, int y, const juce::String& text)
{
    g.setColour (kLime);
    g.fillRoundedRectangle ((float) x, (float) y + 5.0f, 6.0f, 6.0f, 2.0f);
    g.setColour (kText);
    g.setFont (monoFont (12.0f));
    g.drawText (text, x + 12, y, 200, 16, juce::Justification::centredLeft, false);
}

juce::String valueText (const char* id, float v)
{
    const juce::String name (id);
    if (name == "speed")   return juce::String (juce::roundToInt (v)) + " ms";
    if (name == "stab" || name == "mix" || name == "chorus")
        return juce::String (juce::roundToInt (v * 100.0f)) + " %";
    if (name == "range")   return "+/-" + juce::String (juce::roundToInt (v)) + " c";
    if (name == "formant") return juce::String (v, 1) + " st";
    return {};
}

// y of a MIDI note inside the display, clamped to the lanes
float laneY (float midi, int base)
{
    const float y = (float) kPlotY + ((float) kLanes - (midi - (float) base)) * (float) kLaneH - (float) kLaneH * 0.5f;
    return juce::jlimit ((float) kPlotY, (float) (kPlotY + kLanes * kLaneH), y);
}

void levelMeter (juce::Graphics& g, float y, const char* label, float level)
{
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);

    g.setColour (kMuted);
    g.setFont (monoFont (11.0f));
    g.drawText (label, 812, (int) y - 7, 36, 14, juce::Justification::centredLeft, false);

    const juce::Rectangle<float> bar (852.0f, y - 4.0f, 148.0f, 8.0f);
    g.setColour (kPanelHi);
    g.fillRoundedRectangle (bar, 4.0f);
    if (frac > 0.0f)
    {
        g.setColour (kLime);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 4.0f);
    }

    g.setColour (kText);
    g.setFont (monoFont (11.0f, false));
    g.drawText (db <= -59.9f ? juce::String ("-inf") : juce::String (db, 1) + " dB",
                1008, (int) y - 7, 76, 14, juce::Justification::centredRight, false);
}
} // namespace

//==============================================================================
void DMFTuneLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                               bool isHighlighted, bool isDown)
{
    const auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const int kind = (int) b.getProperties().getWithDefault ("kind", 0);

    if (kind == 2) // accent toggle: BYPASS, NEG SPEED
    {
        g.setColour (on ? kPurple : kPanelHi);
        g.fillRoundedRectangle (area, 8.0f);
        g.setColour (on ? kLime : kPurpleGlow);
        g.drawRoundedRectangle (area, 8.0f, 1.5f);
        return;
    }

    if (kind == 3) // preset arrows
    {
        g.setColour (isDown ? kPurple : kPanelHi);
        g.fillRoundedRectangle (area, 8.0f);
        g.setColour (kPurple);
        g.drawRoundedRectangle (area, 8.0f, 1.0f);
        return;
    }

    if (on)
    {
        g.setColour (kLime.withAlpha (0.20f));
        g.fillRoundedRectangle (area.expanded (3.0f), 9.0f);
        g.setColour (kLime);
        g.fillRoundedRectangle (area, 6.0f);
    }
    else
    {
        g.setColour (isDown ? kPurpleDim : (isHighlighted ? kPanelHi : kPanel));
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (isHighlighted ? kPurpleGlow : kPurple);
        g.drawRoundedRectangle (area, 6.0f, 1.0f);
    }
}

void DMFTuneLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    const int kind = (int) b.getProperties().getWithDefault ("kind", 0);

    juce::Colour c = kText;
    if (kind == 2)
        c = on ? kLimeLight : kText;
    else if (on)
        c = kBg;

    g.setColour (c);
    g.setFont (monoFont (kind == 3 ? 16.0f : 11.0f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

void DMFTuneLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (7.0f);
    const float d = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float r = d * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = r - 4.0f;

    // body with a soft shadow
    g.setColour (juce::Colours::black);
    g.fillEllipse (c.x - r, c.y - r + 3.0f, d, d);
    g.setColour (kPanelHi);
    g.fillEllipse (c.x - r, c.y - r, d, d);

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (kPurpleDim);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc: glow then core
    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (kLime.withAlpha (0.22f));
    g.strokePath (value, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (kLime);
    g.strokePath (value, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -(r - 12.0f), 3.0f, r * 0.45f, 1.5f);
    g.setColour (kLimeLight);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));

    // centre cap
    g.setColour (kPanel);
    g.fillEllipse (c.x - r * 0.42f, c.y - r * 0.42f, r * 0.84f, r * 0.84f);
}

//==============================================================================
DMFTuneEditor::DMFTuneEditor (DMFTuneAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
    puffs.reserve (400);

    // KEY
    for (int i = 0; i < 12; ++i)
    {
        keyButton[i].setButtonText (DMFTuneAudioProcessor::noteName (i));
        keyButton[i].onClick = [this, i] { applyKeyAndScale (i, paramIndex ("scale")); };
        addAndMakeVisible (keyButton[i]);
    }

    // SCALE
    const char* scaleNames[5] = { "CHROMATIC", "MAJOR", "MINOR", "MINOR PENTA", "BLUES" };
    for (int i = 0; i < 5; ++i)
    {
        scaleButton[i].setButtonText (scaleNames[i]);
        scaleButton[i].onClick = [this, i] { applyKeyAndScale (paramIndex ("key"), i); };
        addAndMakeVisible (scaleButton[i]);
    }

    // HEATMAP: shows where the tuned notes land (display only)
    heatButton.setButtonText ("HEATMAP");
    heatButton.setClickingTogglesState (true);
    addAndMakeVisible (heatButton);

    // NEG SPEED: instant correction, bound to the parameter
    negButton.setButtonText ("NEG SPEED");
    negButton.getProperties().set ("kind", 2);
    negButton.setClickingTogglesState (true);
    addAndMakeVisible (negButton);
    negAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "negspeed", negButton);

    // BYPASS and preset arrows
    bypassButton.setButtonText ("BYPASS");
    bypassButton.getProperties().set ("kind", 2);
    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "bypass", bypassButton);

    prevButton.setButtonText ("<");
    nextButton.setButtonText (">");
    prevButton.getProperties().set ("kind", 3);
    nextButton.getProperties().set ("kind", 3);
    prevButton.onClick = [this]
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n);
    };
    nextButton.onClick = [this]
    {
        proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % proc.getNumPrograms());
    };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    // KNOBS
    for (int i = 0; i < kNumKnobs; ++i)
    {
        knobSlider[i].setSliderStyle (juce::Slider::RotaryVerticalDrag);
        knobSlider[i].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knobSlider[i].setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                           juce::MathConstants<float>::pi * 2.75f, true);
        knobSlider[i].setMouseDragSensitivity (220);
        addAndMakeVisible (knobSlider[i]);
        knobAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.apvts, kKnobIds[i], knobSlider[i]);
    }

    startTimerHz (30);
    setSize (kW, kH); // last: resized() runs here and needs every component above
}

DMFTuneEditor::~DMFTuneEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
int DMFTuneEditor::paramIndex (const char* id) const
{
    return juce::roundToInt (proc.apvts.getRawParameterValue (id)->load());
}

void DMFTuneEditor::setParam (const char* id, float plain)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

void DMFTuneEditor::applyKeyAndScale (int root, int scale)
{
    static const int scales[5][12] = {
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
        { 0, 2, 4, 5, 7, 9, 11, -1, -1, -1, -1, -1 },
        { 0, 2, 3, 5, 7, 8, 10, -1, -1, -1, -1, -1 },
        { 0, 3, 5, 7, 10, -1, -1, -1, -1, -1, -1, -1 },
        { 0, 3, 5, 6, 7, 10, -1, -1, -1, -1, -1, -1 },
    };
    if (! juce::isPositiveAndBelow (scale, 5))
        return;

    bool on[12] = {};
    for (int k = 0; k < 12; ++k)
        if (scales[scale][k] >= 0)
            on[(root + scales[scale][k]) % 12] = true;

    setParam ("key", (float) root);
    setParam ("scale", (float) scale);
    for (int i = 0; i < 12; ++i)
        setParam (kNoteIds[i], on[i] ? 1.0f : 0.0f);
}

void DMFTuneEditor::timerCallback()
{
    const int key = paramIndex ("key");
    const int scale = paramIndex ("scale");

    for (int i = 0; i < 12; ++i)
        keyButton[i].setToggleState (i == key, juce::dontSendNotification);
    for (int i = 0; i < 5; ++i)
        scaleButton[i].setToggleState (i == scale, juce::dontSendNotification);

    updateDynamics();
    repaint();
}

// Heatmap counts and smoke particles. Both follow the engine's trace, so they move with the voice.
void DMFTuneEditor::updateDynamics()
{
    auto& eng = proc.engine;
    const int head = eng.traceHead();
    const float dt = 1.0f / 30.0f;

    // heatmap: where the tuned notes land, slowly fading
    int from = juce::jmax (lastHeatIdx, head - (dmftune::Engine::kTraceSize - 2));
    from = juce::jmax (from, 0);
    for (int idx = from; idx < head; ++idx)
    {
        const auto tf = eng.traceAt (idx);
        if (tf.inMidi > 0.0f && tf.outMidi > 0.0f)
            heat[(size_t) juce::jlimit (0, 127, juce::roundToInt (tf.outMidi))] += 1.0f;
    }
    lastHeatIdx = head;
    for (auto& h : heat)
        h *= 0.9988f;

    // smoke: spawns on the tuned pitch while singing, billows with the amount of correction
    const float turb = juce::jlimit (0.0f, 1.0f, std::abs (eng.correctionCents()) / 60.0f);
    const auto last = eng.traceAt (head - 1);

    if (last.inMidi > 0.0f && last.outMidi > 0.0f)
    {
        const float yTuned = laneY (last.outMidi, displayBase);
        for (int k = 0; k < 2; ++k)
        {
            Puff pf;
            pf.x = (float) (kPlotX + kPlotW - 2);
            pf.y = yTuned + rng.nextFloat() * 6.0f - 3.0f;
            pf.vy = (rng.nextFloat() - 0.5f) * (10.0f + 60.0f * turb);
            pf.maxLife = 1.6f + rng.nextFloat();
            pf.life = pf.maxLife;
            pf.size = 5.0f + 6.0f * turb + 3.0f * rng.nextFloat();
            puffs.push_back (pf);
        }
    }

    const float scrollPx = (float) kPlotW / kWinSeconds;  // px per second, same as the trace
    for (auto& pf : puffs)
    {
        pf.x -= scrollPx * dt;
        pf.y += pf.vy * dt;
        pf.vy += ((rng.nextFloat() - 0.5f) * (40.0f + 160.0f * turb) - 8.0f) * dt;  // rises slowly
        pf.vy *= 0.97f;
        pf.size += (6.0f + 10.0f * turb) * dt;
        pf.life -= dt;
    }

    puffs.erase (std::remove_if (puffs.begin(), puffs.end(),
                                 [] (const Puff& pf) { return pf.life <= 0.0f || pf.x < (float) kPlotX - 30.0f; }),
                 puffs.end());
    if (puffs.size() > 360)
        puffs.erase (puffs.begin(), puffs.begin() + (std::ptrdiff_t) (puffs.size() - 360));
}

//==============================================================================
void DMFTuneEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c0a33), 0.0f, 0.0f,
                                             kBg, bounds.getWidth(), bounds.getHeight(), false));
    g.fillRect (bounds);

    // header
    g.setColour (kPanelLo);
    g.fillRect (juce::Rectangle<float> (0.0f, 0.0f, bounds.getWidth(), 96.0f));
    glowLine (g, 0.0f, 96.0f, bounds.getWidth(), 96.0f, kLime);

    if (logo.isValid())
        g.drawImageWithin (logo, 14, 8, 80, 80, juce::RectanglePlacement::centred, false);

    g.setFont (titleFont (40.0f));
    g.setColour (kLime.withAlpha (0.30f));
    g.drawText ("DMF TUNE", 107, 11, 420, 48, juce::Justification::centredLeft, false);
    g.setColour (kLime);
    g.drawText ("DMF TUNE", 105, 9, 420, 48, juce::Justification::centredLeft, false);

    g.setColour (kMuted);
    g.setFont (monoFont (11.0f, false));
    g.drawText ("AUTO PITCH CORRECTION   //   DMF LAB", 107, 58, 460, 16, juce::Justification::centredLeft, false);

    // preset name
    g.setColour (kPanel);
    g.fillRoundedRectangle (600.0f, 30.0f, 220.0f, 36.0f, 8.0f);
    g.setColour (kPurple);
    g.drawRoundedRectangle (600.5f, 30.5f, 219.0f, 35.0f, 8.0f, 1.0f);
    g.setColour (kText);
    g.setFont (monoFont (13.0f));
    g.drawText (proc.getProgramName (proc.getCurrentProgram()), 600, 30, 220, 36,
                juce::Justification::centred, false);

    // panels
    panel (g, juce::Rectangle<float> (20.0f, 112.0f, 760.0f, 358.0f));
    panel (g, juce::Rectangle<float> (796.0f, 112.0f, 304.0f, 358.0f));
    panel (g, juce::Rectangle<float> (20.0f, 486.0f, 1080.0f, 128.0f));

    paintDisplay (g);
    paintRightPanel (g);
    paintBottom (g);

    // footer
    g.setColour (kMuted);
    g.setFont (monoFont (10.0f, false));
    g.drawText ("DMF TUNE  v1.1", 22, 622, 300, 16, juce::Justification::centredLeft, false);
    g.drawText ("DMF LAB", kW - 322, 622, 300, 16, juce::Justification::centredRight, false);

    // outer frame
    g.setColour (kPurple.withAlpha (0.8f));
    g.drawRoundedRectangle (bounds.reduced (6.0f), 14.0f, 1.5f);
}

void DMFTuneEditor::paintSmoke (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (juce::Rectangle<int> (kPlotX, kPlotY, kPlotW, kLanes * kLaneH));

    for (const auto& pf : puffs)
    {
        const float age = 1.0f - pf.life / pf.maxLife;               // 0 = new, 1 = gone
        const float alpha = juce::jmin (1.0f, age * 6.0f) * (1.0f - age) * 0.35f;
        const float r = pf.size * (1.0f + age * 0.8f);

        g.setGradientFill (juce::ColourGradient (kLime.withAlpha (alpha), pf.x, pf.y,
                                                 kLime.withAlpha (0.0f), pf.x + r, pf.y, true));
        g.fillEllipse (pf.x - r, pf.y - r, 2.0f * r, 2.0f * r);
    }
}

void DMFTuneEditor::paintDisplay (juce::Graphics& g)
{
    auto& eng = proc.engine;
    const bool heatOn = heatButton.getToggleState();

    const uint16_t mask = [this]
    {
        uint16_t m = 0;
        for (int i = 0; i < 12; ++i)
            if (proc.apvts.getRawParameterValue (kNoteIds[i])->load() > 0.5f)
                m = (uint16_t) (m | (1u << i));
        return m;
    }();

    sectionLabel (g, 36, 122, "PITCH");

    // legend
    g.setFont (monoFont (11.0f, false));
    g.setColour (kMuted);
    g.drawText ("RAW", 256, 122, 40, 16, juce::Justification::centredLeft, false);
    g.drawText ("TUNED", 326, 122, 60, 16, juce::Justification::centredLeft, false);
    {
        juce::Path p, dashed;
        p.startNewSubPath (232.0f, 130.0f);
        p.lineTo (250.0f, 130.0f);
        const float dash[] = { 4.0f, 3.0f };
        juce::PathStrokeType (2.0f).createDashedStroke (dashed, p, dash, 2);
        g.setColour (kPurpleGlow);
        g.fillPath (dashed);
        g.setColour (kLime);
        g.fillRoundedRectangle (300.0f, 127.0f, 18.0f, 4.0f, 2.0f);
    }

    // follow the singer's register
    const int head = eng.traceHead();
    {
        float sum = 0.0f;
        int cnt = 0;
        for (int k = 1; k <= 80; ++k)
        {
            const auto tf = eng.traceAt (head - k);
            if (tf.inMidi > 0.0f) { sum += tf.inMidi; ++cnt; }
        }
        if (cnt > 10)
        {
            const float mean = sum / (float) cnt;
            if (mean < (float) displayBase + 4.0f || mean > (float) displayBase + 20.0f)
                displayBase = juce::jlimit (24, 84, 12 * (int) std::floor ((mean - 6.0f) / 12.0f));
        }
    }

    float maxHeat = 1.0f;
    for (const float h : heat)
        maxHeat = juce::jmax (maxHeat, h);

    // lanes (+ heatmap tint)
    for (int i = 0; i < kLanes; ++i)
    {
        const int p = kLanes - 1 - i;
        const int midi = displayBase + p;
        const int pc = ((midi % 12) + 12) % 12;
        const bool on = ((mask >> pc) & 1) != 0;
        const bool black = ((kBlackKeys >> pc) & 1) != 0;
        const int y = kPlotY + i * kLaneH;

        g.setColour (on ? kLime.withAlpha (0.10f) : (black ? juce::Colour (0xff08050c) : juce::Colour (0xff120c1a)));
        g.fillRect (kPlotX, y, kPlotW, kLaneH);

        if (heatOn)
        {
            const float hv = juce::jlimit (0.0f, 1.0f, heat[(size_t) juce::jlimit (0, 127, midi)] / maxHeat);
            if (hv > 0.01f)
            {
                g.setColour (kLime.withAlpha (0.08f + 0.6f * hv));
                g.fillRect (kPlotX, y, kPlotW, kLaneH);
            }
        }

        g.setColour (kGrid);
        g.fillRect (kPlotX, y + kLaneH - 1, kPlotW, 1);

        const juce::String label = pc == 0 ? juce::String ("C") + juce::String (midi / 12 - 1)
                                           : juce::String (DMFTuneAudioProcessor::noteName (pc));
        g.setColour (on ? kLime : kMuted);
        g.setFont (monoFont (9.5f));
        g.drawText (label, kPlotX - 36, y - 1, 32, kLaneH + 2, juce::Justification::centredRight, false);
    }

    // time grid
    g.setColour (kGrid);
    for (int k = 1; k <= 3; ++k)
        g.fillRect (kPlotX + k * 170, kPlotY, 1, kLanes * kLaneH);

    // green smoke, under the trace lines
    paintSmoke (g);

    // traces
    const int nFrames = juce::jmin (eng.kTraceSize - 2, (int) (kWinSeconds / eng.hopSeconds()));
    juce::Path raw, tuned;
    bool rawOpen = false, tunedOpen = false;
    for (int k = 0; k < nFrames; ++k)
    {
        const int idx = head - nFrames + k;
        const float x = (float) kPlotX + 6.0f + (float) k / (float) (nFrames - 1) * (float) (kPlotW - 6);
        const auto tf = idx >= 0 ? eng.traceAt (idx) : dmftune::TraceFrame();
        if (tf.inMidi > 0.0f)
        {
            const float yRaw = laneY (tf.inMidi, displayBase);
            const float yTun = laneY (tf.outMidi, displayBase);
            if (rawOpen)   raw.lineTo (x, yRaw);
            else         { raw.startNewSubPath (x, yRaw); rawOpen = true; }
            if (tunedOpen) tuned.lineTo (x, yTun);
            else         { tuned.startNewSubPath (x, yTun); tunedOpen = true; }
        }
        else
        {
            rawOpen = false;
            tunedOpen = false;
        }
    }

    if (! raw.isEmpty())
    {
        juce::Path dashed;
        const float dash[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, raw, dash, 2);
        g.setColour (kPurpleGlow.withAlpha (0.75f));
        g.fillPath (dashed);

        g.setColour (kLime.withAlpha (0.20f));
        g.strokePath (tuned, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (kLime);
        g.strokePath (tuned, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // NOW line
    g.setColour (kPurpleGlow);
    g.fillRect (kPlotX + kPlotW - 2, kPlotY, 2, kLanes * kLaneH);

    // target readout
    {
        juce::String txt = "--";
        for (int k = 1; k <= 12; ++k)
        {
            const auto tf = eng.traceAt (head - k);
            if (tf.inMidi > 0.0f && tf.targetMidi > 0.0f)
            {
                const int tm = juce::roundToInt (tf.targetMidi);
                const int cents = juce::roundToInt ((tf.inMidi - tf.targetMidi) * 100.0f);
                txt = juce::String (DMFTuneAudioProcessor::noteName (tm)) + juce::String (tm / 12 - 1)
                      + "   " + juce::String (cents > 0 ? "+" : "") + juce::String (cents) + " c";
                break;
            }
        }
        g.setColour (kLime);
        g.setFont (monoFont (13.0f));
        g.drawText (txt, 460, 120, 200, 18, juce::Justification::centredRight, false);
    }

    // time axis
    g.setFont (monoFont (10.0f, false));
    const char* ticks[] = { "-8s", "-6s", "-4s", "-2s" };
    g.setColour (kMuted);
    for (int k = 0; k < 4; ++k)
        g.drawText (ticks[k], kPlotX + k * 170 + 4, kPlotY + kLanes * kLaneH + 4, 40, 14,
                    juce::Justification::centredLeft, false);
    g.setColour (kLime);
    g.drawText ("NOW", kPlotX + kPlotW - 40, kPlotY + kLanes * kLaneH + 4, 36, 14,
                juce::Justification::centredRight, false);
}

void DMFTuneEditor::paintRightPanel (juce::Graphics& g)
{
    sectionLabel (g, 812, 122, "CONTROLS");

    for (int i = 0; i < kNumKnobs; ++i)
    {
        const int col = i % 3, row = i / 3;
        const int cx = 858 + col * 100;
        const int top = row == 0 ? 146 : 266;
        const float v = proc.apvts.getRawParameterValue (kKnobIds[i])->load();

        g.setColour (kText);
        g.setFont (monoFont (11.0f));
        g.drawText (kKnobNames[i], cx - 50, top + 72, 100, 14, juce::Justification::centred, false);
        g.setColour (kMuted);
        g.setFont (monoFont (10.0f, false));
        g.drawText (valueText (kKnobIds[i], v), cx - 50, top + 88, 100, 14, juce::Justification::centred, false);
    }

    g.setColour (kMuted);
    g.setFont (monoFont (10.0f, false));
    g.drawText ("NEG SPEED: instant correction, hard tuning", 812, 438, 272, 14,
                juce::Justification::centred, false);
}

void DMFTuneEditor::paintBottom (juce::Graphics& g)
{
    sectionLabel (g, 36, 494, "KEY");
    sectionLabel (g, 36, 552, "SCALE");

    // correction meter
    const float corr = juce::jlimit (-50.0f, 50.0f, proc.engine.correctionCents());
    sectionLabel (g, 812, 494, "CORRECTION");
    g.setColour (kLime);
    g.setFont (monoFont (12.0f));
    g.drawText (juce::String (corr > 0.5f ? "+" : "") + juce::String (juce::roundToInt (corr)) + " c",
                980, 494, 104, 16, juce::Justification::centredRight, false);

    const juce::Rectangle<float> track (812.0f, 520.0f, 272.0f, 10.0f);
    g.setColour (kPanelHi);
    g.fillRoundedRectangle (track, 5.0f);
    const float mid = track.getCentreX();
    const float half = track.getWidth() * 0.5f;
    const float wBar = std::abs (corr) / 50.0f * half;
    if (wBar > 0.0f)
    {
        g.setColour (kLime);
        g.fillRoundedRectangle (juce::Rectangle<float> (corr >= 0.0f ? mid : mid - wBar, track.getY(),
                                                        wBar, track.getHeight()), 4.0f);
    }
    g.setColour (kText);
    g.fillRect (mid - 0.5f, track.getY() - 3.0f, 1.0f, track.getHeight() + 6.0f);

    g.setColour (kMuted);
    g.setFont (monoFont (10.0f, false));
    g.drawText ("-50", 812, 534, 60, 14, juce::Justification::centredLeft, false);
    g.drawText ("+50", 1024, 534, 60, 14, juce::Justification::centredRight, false);

    levelMeter (g, 560.0f, "IN", proc.inLevel.load());
    levelMeter (g, 584.0f, "OUT", proc.outLevel.load());
}

void DMFTuneEditor::resized()
{
    for (int i = 0; i < 12; ++i)
        keyButton[i].setBounds (36 + i * 62, 514, 56, 28);

    for (int i = 0; i < 5; ++i)
        scaleButton[i].setBounds (36 + i * 148, 572, 140, 26);

    heatButton.setBounds (672, 118, 92, 24);

    for (int i = 0; i < kNumKnobs; ++i)
    {
        const int col = i % 3, row = i / 3;
        const int cx = 858 + col * 100;
        const int top = row == 0 ? 146 : 266;
        knobSlider[i].setBounds (cx - 34, top, 68, 68);
    }

    negButton.setBounds (812, 396, 272, 36);

    prevButton.setBounds (560, 30, 36, 36);
    nextButton.setBounds (824, 30, 36, 36);
    bypassButton.setBounds (kW - 160, 30, 136, 36);
}
