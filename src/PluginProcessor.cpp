#include "PluginProcessor.h"
#include "PluginEditor.h"

CanaryAudioProcessor::CanaryAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",     juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput("Output",   juce::AudioChannelSet::stereo(), true))
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

    auto sidechain = getBusBuffer(buffer, true, 1);
    sidechainConnected.store(sidechain.getNumChannels() > 0);

    // Phase 1: passthrough -- main input buffer IS the output buffer
    // No processing needed; buffer passes through unmodified
}

juce::AudioProcessorEditor* CanaryAudioProcessor::createEditor()
{
    return new CanaryAudioProcessorEditor(*this);
}

void CanaryAudioProcessor::getStateInformation(juce::MemoryBlock& /*destData*/)
{
    // Stub for Phase 1
}

void CanaryAudioProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/)
{
    // Stub for Phase 1
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CanaryAudioProcessor();
}
