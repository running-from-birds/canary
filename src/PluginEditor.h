#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class CanaryAudioProcessorEditor : public juce::AudioProcessorEditor
                                 , private juce::Timer
{
public:
    explicit CanaryAudioProcessorEditor(CanaryAudioProcessor&);
    ~CanaryAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void updateSidechainStatus();

    void configureSlider(juce::Slider& slider,
                         juce::Label& label,
                         const juce::String& labelText,
                         const juce::String& parameterId);

    CanaryAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& parameters;

    juce::Slider bandsSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider formantSlider;
    juce::Slider dryWetSlider;
    juce::Slider gainSlider;

    juce::Label bandsLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label formantLabel;
    juce::Label dryWetLabel;
    juce::Label gainLabel;
    juce::Label sidechainLabel;

    std::unique_ptr<SliderAttachment> bandsAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> formantAttachment;
    std::unique_ptr<SliderAttachment> dryWetAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CanaryAudioProcessorEditor)
};
