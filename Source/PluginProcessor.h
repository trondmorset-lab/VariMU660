#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "VariMuCompressor.h"

namespace ParamIDs
{
    inline constexpr auto input     = "input";
    inline constexpr auto threshold = "threshold";
    inline constexpr auto timeConst = "timeconst";
    inline constexpr auto output    = "output";
    inline constexpr auto color     = "color";
    inline constexpr auto mix       = "mix";
    inline constexpr auto link      = "link";
}

class VariMuAudioProcessor : public juce::AudioProcessor
{
public:
    VariMuAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> gainReductionDb { 0.0f };   // leses av VU-måleren

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    varimu::Compressor compressor;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::Gain<float> inputGain, outputGain;
    juce::dsp::DryWetMixer<float> dryWet { 256 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> bypassDelay { 256 };

    std::atomic<float>* input     = nullptr;
    std::atomic<float>* threshold = nullptr;
    std::atomic<float>* timeConst = nullptr;
    std::atomic<float>* output    = nullptr;
    std::atomic<float>* color     = nullptr;
    std::atomic<float>* mix       = nullptr;
    std::atomic<float>* link      = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VariMuAudioProcessor)
};
