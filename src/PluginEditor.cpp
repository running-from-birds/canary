#include "PluginEditor.h"

CanaryAudioProcessorEditor::CanaryAudioProcessorEditor(CanaryAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), parameters(p.getValueTreeState())
{
    setSize(450, 380);

    configureSlider(bandsSlider, bandsLabel, "Bands", "bands");
    configureSlider(attackSlider, attackLabel, "Attack", "attack");
    configureSlider(releaseSlider, releaseLabel, "Release", "release");
    configureSlider(formantSlider, formantLabel, "Formant", "formantShift");
    configureSlider(dryWetSlider, dryWetLabel, "Dry/Wet", "dryWet");
    configureSlider(gainSlider, gainLabel, "Gain", "outputGain");

    bandsAttachment = std::make_unique<SliderAttachment>(parameters, "bands", bandsSlider);
    attackAttachment = std::make_unique<SliderAttachment>(parameters, "attack", attackSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(parameters, "release", releaseSlider);
    formantAttachment = std::make_unique<SliderAttachment>(parameters, "formantShift", formantSlider);
    dryWetAttachment = std::make_unique<SliderAttachment>(parameters, "dryWet", dryWetSlider);
    gainAttachment = std::make_unique<SliderAttachment>(parameters, "outputGain", gainSlider);

    sidechainLabel.setJustificationType(juce::Justification::centred);
    sidechainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(sidechainLabel);
    updateSidechainStatus();

    startTimerHz(15);
}

CanaryAudioProcessorEditor::~CanaryAudioProcessorEditor() {}

void CanaryAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111317));
    g.setColour(juce::Colour(0xffe4f2ff));
    g.setFont(24.0f);
    g.drawText("Canary", 0, 8, getWidth(), 30, juce::Justification::centred);
}

void CanaryAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(16);
    bounds.removeFromTop(44);

    auto controls = bounds.removeFromTop(250);
    const auto rowHeight = controls.getHeight() / 2;

    auto topRow = controls.removeFromTop(rowHeight);
    auto bottomRow = controls;

    auto layoutRow = [](juce::Rectangle<int> row, juce::Slider& s1, juce::Slider& s2, juce::Slider& s3)
    {
        const auto cellWidth = row.getWidth() / 3;
        auto c1 = row.removeFromLeft(cellWidth).reduced(8);
        auto c2 = row.removeFromLeft(cellWidth).reduced(8);
        auto c3 = row.reduced(8);

        s1.setBounds(c1);
        s2.setBounds(c2);
        s3.setBounds(c3);
    };

    layoutRow(topRow, bandsSlider, attackSlider, releaseSlider);
    layoutRow(bottomRow, formantSlider, dryWetSlider, gainSlider);

    sidechainLabel.setBounds(bounds.removeFromBottom(28));
}

void CanaryAudioProcessorEditor::timerCallback()
{
    updateSidechainStatus();
}

void CanaryAudioProcessorEditor::updateSidechainStatus()
{
    if (!processorRef.isSidechainConnected())
    {
        sidechainLabel.setText("Sidechain: Not Connected", juce::dontSendNotification);
        sidechainLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff87171));
        return;
    }

    if (processorRef.isSidechainSignalPresent())
    {
        sidechainLabel.setText("Sidechain: Signal Detected", juce::dontSendNotification);
        sidechainLabel.setColour(juce::Label::textColourId, juce::Colour(0xff34d399));
        return;
    }

    sidechainLabel.setText("Sidechain: Connected (No Signal)", juce::dontSendNotification);
    sidechainLabel.setColour(juce::Label::textColourId, juce::Colour(0xfffbbf24));
}

void CanaryAudioProcessorEditor::configureSlider(juce::Slider& slider,
                                                 juce::Label& label,
                                                 const juce::String& labelText,
                                                 const juce::String&)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff3b82f6));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2a3038));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffd6ecff));
    addAndMakeVisible(slider);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredTop);
    label.attachToComponent(&slider, false);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffcfd6dd));
    addAndMakeVisible(label);
}
