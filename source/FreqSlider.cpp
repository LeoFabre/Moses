#include "FreqSlider.h"

FreqSlider::FreqSlider(PluginProcessor& p, juce::String paramID) : processorRef(p)
{
    mFreqSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    mFreqSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    mFreqSlider.setRange(-60.0f, 0.0f, 0.01);
    mFreqSlider.setValue(0.0);
    addAndMakeVisible(mFreqSlider);

    mFreqLabel.setFont(15.0f);
    mFreqLabel.setText("Freq", juce::dontSendNotification);
    mFreqLabel.setJustificationType(juce::Justification::centredTop);
    mFreqLabel.attachToComponent(&mFreqSlider, false);

    mFreqSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), paramID, mFreqSlider);
}

FreqSlider::~FreqSlider() = default;

void FreqSlider::paint(juce::Graphics &g) {
    Component::paint(g);
}

void FreqSlider::resized() {
    mFreqSlider.setBoundsRelative(0, 0.3, 1, 0.7);
}
