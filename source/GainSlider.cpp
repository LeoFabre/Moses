#include "GainSlider.h"

GainSlider::GainSlider(PluginProcessor& p, juce::String paramID) : processorRef(p)
{
    mGainSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    mGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    mGainSlider.setRange(-60.0f, 0.0f, 0.01);
    mGainSlider.setValue(0.0);
    addAndMakeVisible(mGainSlider);

    mGainLabel.setFont(15.0f);
    mGainLabel.setText("Gain", juce::dontSendNotification);
    mGainLabel.setJustificationType(juce::Justification::centredTop);
    mGainLabel.attachToComponent(&mGainSlider, false);

    mGainSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), paramID, mGainSlider);
}

GainSlider::~GainSlider() = default;

void GainSlider::paint(juce::Graphics &g) {
    Component::paint(g);
}

void GainSlider::resized() {
    mGainSlider.setBoundsRelative(0, 0.3, 1, 0.7);
}
