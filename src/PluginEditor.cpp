#include "PluginEditor.h"

CanaryAudioProcessorEditor::CanaryAudioProcessorEditor(CanaryAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(450, 380);
}

CanaryAudioProcessorEditor::~CanaryAudioProcessorEditor() {}

void CanaryAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawFittedText("Canary", getLocalBounds(), juce::Justification::centred, 1);
}

void CanaryAudioProcessorEditor::resized() {}
