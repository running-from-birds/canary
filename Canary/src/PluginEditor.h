#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class CanaryAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit CanaryAudioProcessorEditor(CanaryAudioProcessor&);
    ~CanaryAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    CanaryAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CanaryAudioProcessorEditor)
};
