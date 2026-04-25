#include "PluginProcessor.h"
#include "PluginEditor.h"

CanaryAudioProcessor::CanaryAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",     juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput("Output",   juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "CanaryParameters", createParameterLayout())
{
}

CanaryAudioProcessor::~CanaryAudioProcessor() {}

void CanaryAudioProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
    // No DSP to prepare in Phase 1
}

void CanaryAudioProcessor::releaseResources()
{
    // Nothing to release in Phase 1
}

bool CanaryAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Main input must be mono or stereo
    const auto& mainInput = layouts.getMainInputChannelSet();
    if (mainInput != juce::AudioChannelSet::stereo()
        && mainInput != juce::AudioChannelSet::mono())
        return false;

    // Output must match main input
    if (layouts.getMainOutputChannelSet() != mainInput)
        return false;

    // Sidechain: accept stereo, mono, or disabled
    const auto& sidechain = layouts.getChannelSet(true, 1);
    if (!sidechain.isDisabled()
        && sidechain != juce::AudioChannelSet::stereo()
        && sidechain != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void CanaryAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainInput = getBusBuffer(buffer, true, 0);
    auto mainOutput = getBusBuffer(buffer, false, 0);
    auto sidechain = getBusBuffer(buffer, true, 1);

    const auto sideChannels = sidechain.getNumChannels();
    const bool hasSidechain = sideChannels > 0;
    sidechainConnected.store(hasSidechain, std::memory_order_relaxed);

    bool hasAudibleSidechain = false;
    if (hasSidechain)
    {
        for (int ch = 0; ch < sideChannels; ++ch)
        {
            if (sidechain.getRMSLevel(ch, 0, sidechain.getNumSamples()) > 1.0e-4f)
            {
                hasAudibleSidechain = true;
                break;
            }
        }
    }
    sidechainSignalPresent.store(hasAudibleSidechain, std::memory_order_relaxed);

    const auto dryWet = juce::jlimit(0.0f, 1.0f,
                                     parameters.getRawParameterValue("dryWet")->load());
    const auto outputGainDb = parameters.getRawParameterValue("outputGain")->load();
    const auto outputGain = juce::Decibels::decibelsToGain(outputGainDb);

    const auto outputChannels = mainOutput.getNumChannels();
    const auto numSamples = mainOutput.getNumSamples();

    for (int ch = 0; ch < outputChannels; ++ch)
    {
        auto* out = mainOutput.getWritePointer(ch);
        const auto* in = mainInput.getReadPointer(ch);
        const auto sideIndex = hasSidechain ? juce::jmin(ch, sideChannels - 1) : 0;
        const auto* side = hasSidechain ? sidechain.getReadPointer(sideIndex) : nullptr;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto dry = in[sample];
            const auto wet = hasSidechain ? side[sample] : dry;
            out[sample] = ((1.0f - dryWet) * dry + (dryWet * wet)) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* CanaryAudioProcessor::createEditor()
{
    return new CanaryAudioProcessorEditor(*this);
}

void CanaryAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.state.createXml())
        copyXmlToBinary(*xml, destData);
}

void CanaryAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr)
        return;

    if (xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

CanaryAudioProcessor::APVTS::ParameterLayout CanaryAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterInt>(
        "bands", "Bands", 8, 32, 16));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "attack", "Attack",
        juce::NormalisableRange<float>(1.0f, 50.0f, 0.1f), 5.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "release", "Release",
        juce::NormalisableRange<float>(10.0f, 200.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "formantShift", "Formant Shift",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("st")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "dryWet", "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int)
                                         { return juce::String(juce::roundToInt(value * 100.0f)) + " %"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "outputGain", "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CanaryAudioProcessor();
}
