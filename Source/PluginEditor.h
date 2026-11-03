#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Svart bakelittknott med hvit peker og skala – i Fairchild-stil
class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// VU-måler som viser gain-reduksjon (nålen hviler på 0 og faller mot venstre)
class GainReductionMeter : public juce::Component, private juce::Timer
{
public:
    explicit GainReductionMeter (std::atomic<float>& source);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    static float dbToPosition (float grDb);

    std::atomic<float>& grSource;
    float needleDb = 0.0f;
};

class VariMuAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VariMuAudioProcessorEditor (VariMuAudioProcessor&);
    ~VariMuAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::String label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& label);
    void drawKnobScale (juce::Graphics&, const Knob&, int numTicks, const juce::StringArray& labels);

    VariMuAudioProcessor& audioProcessor;
    VintageLookAndFeel lnf;
    juce::Image logo;

    Knob input, threshold, timeConst, output, color, mix;
    juce::ToggleButton linkButton { "STEREO LINK" };
    std::unique_ptr<ButtonAttachment> linkAttachment;
    GainReductionMeter meter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VariMuAudioProcessorEditor)
};
