#pragma once
#include "juce_gui_basics/juce_gui_basics.h"
#include "PluginProcessor.h"

class GainSlider : public juce::Component
{
public:

    GainSlider(PluginProcessor &p, juce::String paramID);

    ~GainSlider() override;
    void paint(juce::Graphics &g) override;
    void resized() override;

private:
    juce::Slider mGainSlider;
    juce::Label mGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mGainSliderAttachment;

    PluginProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainSlider)
};
