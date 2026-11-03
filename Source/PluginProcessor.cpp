#include "PluginProcessor.h"
#include "PluginEditor.h"

VariMuAudioProcessor::VariMuAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    input     = apvts.getRawParameterValue (ParamIDs::input);
    threshold = apvts.getRawParameterValue (ParamIDs::threshold);
    timeConst = apvts.getRawParameterValue (ParamIDs::timeConst);
    output    = apvts.getRawParameterValue (ParamIDs::output);
    color     = apvts.getRawParameterValue (ParamIDs::color);
    mix       = apvts.getRawParameterValue (ParamIDs::mix);
    link      = apvts.getRawParameterValue (ParamIDs::link);
}

juce::AudioProcessorValueTreeState::ParameterLayout VariMuAudioProcessor::createLayout()
{
    using namespace juce;

    auto withUnit = [] (const char* unit, int decimals)
    {
        return AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (
            [=] (float v, int) { return String (v, decimals) + " " + unit; });
    };

    return {
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::input, 1 }, "Input Gain",
            NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f, withUnit ("dB", 1)),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::threshold, 1 }, "Threshold",
            NormalisableRange<float> (0.0f, 10.0f, 0.01f), 4.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })),
        std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::timeConst, 1 }, "Time Constant",
            StringArray { "1", "2", "3", "4", "5", "6" }, 0),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output Gain",
            NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, withUnit ("dB", 1)),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::color, 1 }, "Tube Color",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, withUnit ("%", 0)),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, withUnit ("%", 0)),
        std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::link, 1 }, "Stereo Link", true),
    };
}

bool VariMuAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && out == layouts.getMainInputChannelSet();
}

void VariMuAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = (juce::uint32) getTotalNumOutputChannels();
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, numChannels };

    // 2x oversampling med lineærfase FIR, så rørmetningen ikke aliaserer
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);
    compressor.prepare (sampleRate * 2.0);

    // Fast latens: rapporteres til verten og legges på dry- og bypass-signalet
    const int latency = (int) std::round (oversampling->getLatencyInSamples());
    setLatencySamples (latency);

    for (auto* gain : { &inputGain, &outputGain })
    {
        gain->prepare (spec);
        gain->setRampDurationSeconds (0.05);
    }

    dryWet.prepare (spec);
    dryWet.setWetLatency ((float) latency);

    bypassDelay.prepare (spec);
    bypassDelay.setDelay ((float) latency);

    reset();
}

void VariMuAudioProcessor::reset()
{
    if (oversampling != nullptr)
        oversampling->reset();
    compressor.reset();
    inputGain.reset();
    outputGain.reset();
    dryWet.reset();
    bypassDelay.reset();
    gainReductionDb = 0.0f;
}

void VariMuAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumSamples() == 0)
        return;

    compressor.setParams ({ threshold->load(),
                            (int) timeConst->load() + 1,
                            color->load() * 0.01f,
                            link->load() > 0.5f });
    inputGain.setGainDecibels (input->load());
    outputGain.setGainDecibels (output->load());
    dryWet.setWetMixProportion (mix->load() * 0.01f);

    juce::dsp::AudioBlock<float> block (buffer);
    dryWet.pushDrySamples (block);
    inputGain.process (juce::dsp::ProcessContextReplacing<float> (block));

    auto up = oversampling->processSamplesUp (block);
    float* channels[varimu::Compressor::maxChannels] {};
    const int numUp = (int) juce::jmin (up.getNumChannels(), (size_t) varimu::Compressor::maxChannels);
    for (int ch = 0; ch < numUp; ++ch)
        channels[ch] = up.getChannelPointer ((size_t) ch);

    gainReductionDb = compressor.process (channels, numUp, (int) up.getNumSamples());

    oversampling->processSamplesDown (block);
    outputGain.process (juce::dsp::ProcessContextReplacing<float> (block));
    dryWet.mixWetSamples (block);
}

// Bypass: send lyden uendret ut, men forsinket like mye som den prosesserte
// lyden, så verten kompenserer riktig og lyden ikke hopper når bypass slås av/på.
void VariMuAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::dsp::AudioBlock<float> block (buffer);
    bypassDelay.process (juce::dsp::ProcessContextReplacing<float> (block));
    gainReductionDb = 0.0f;
}

juce::AudioProcessorEditor* VariMuAudioProcessor::createEditor()
{
    return new VariMuAudioProcessorEditor (*this);
}

void VariMuAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VariMuAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VariMuAudioProcessor();
}
