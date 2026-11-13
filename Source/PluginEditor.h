#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

#include "PluginProcessor.h"

//==============================================================================
class DMFTuneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};

//==============================================================================
class DMFTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DMFTuneEditor (DMFTuneAudioProcessor&);
    ~DMFTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kNumKnobs = 6;

    // one smoke puff of the green cloud that follows the tuned pitch
    struct Puff
    {
        float x = 0.0f, y = 0.0f, vy = 0.0f;
        float life = 0.0f, maxLife = 1.0f, size = 6.0f;
    };

    void timerCallback() override;
    int paramIndex (const char* id) const;
    void setParam (const char* id, float plainValue);
    void applyKeyAndScale (int root, int scale);
    void updateDynamics();

    void paintDisplay (juce::Graphics&);
    void paintSmoke (juce::Graphics&);
    void paintRightPanel (juce::Graphics&);
    void paintBottom (juce::Graphics&);

    DMFTuneAudioProcessor& proc;
    DMFTuneLookAndFeel look;   // declared before the components so it outlives them
    juce::Image logo;
    juce::Random rng;
    int displayBase = 48;      // MIDI note at the bottom lane of the pitch display
    int lastHeatIdx = -1;      // last trace frame counted in the heatmap
    std::array<float, 128> heat {};
    std::vector<Puff> puffs;

    juce::TextButton keyButton[12];
    juce::TextButton scaleButton[5];
    juce::TextButton heatButton, negButton, bypassButton, prevButton, nextButton;
    juce::Slider knobSlider[kNumKnobs];

    // attachments are declared after the components they point to
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> negAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttach[kNumKnobs];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DMFTuneEditor)
};
