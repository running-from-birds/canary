#include "PluginEditor.h"

ChordVocoderEditor::ChordVocoderEditor(ChordVocoderProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(450, 380);
}

ChordVocoderEditor::~ChordVocoderEditor() {}

void ChordVocoderEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawFittedText("ChordVocoder", getLocalBounds(), juce::Justification::centred, 1);
}

void ChordVocoderEditor::resized() {}
