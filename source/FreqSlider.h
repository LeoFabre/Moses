#pragma once
#include "juce_gui_basics/juce_gui_basics.h"
#include "PluginProcessor.h"

class FreqSlider : public juce::Component
{
public:
    FreqSlider(PluginProcessor& p, juce::String paramID);
    ~FreqSlider() override;
    void paint(juce::Graphics &g) override;
    void resized() override;

private:
    juce::Slider mFreqSlider;
    juce::Label mFreqLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mFreqSliderAttachment;

    PluginProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreqSlider)
};
