#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class CanaryAudioProcessor : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    CanaryAudioProcessor();
    ~CanaryAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    APVTS& getValueTreeState() { return parameters; }
    const APVTS& getValueTreeState() const { return parameters; }

    bool isSidechainSignalPresent() const
    {
        return sidechainSignalPresent.load(std::memory_order_relaxed);
    }

    bool isSidechainConnected() const
    {
        return sidechainConnected.load(std::memory_order_relaxed);
    }

private:
    static APVTS::ParameterLayout createParameterLayout();

    APVTS parameters;

    // Cross-thread status flags (audio thread writes, GUI reads).
    std::atomic<bool> sidechainConnected { false };
    std::atomic<bool> sidechainSignalPresent { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CanaryAudioProcessor)
};
