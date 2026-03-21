#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class ChordVocoderEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChordVocoderEditor(ChordVocoderProcessor&);
    ~ChordVocoderEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ChordVocoderProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordVocoderEditor)
};
