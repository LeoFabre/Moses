#pragma once

#include "FreqSlider.h"
#include "GainSlider.h"
#include "PluginProcessor.h"

//==============================================================================
class PluginEditor
        : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor &);

    ~PluginEditor() override;

    //==============================================================================
    void paint(juce::Graphics &) override;

    void resized() override;

private:
    PluginProcessor &processorRef;

    static constexpr int numBands = 5;  // Nombre de bandes pour le crossover
    juce::OwnedArray<GainSlider> gainSliders;
    juce::OwnedArray<FreqSlider> freqSliders;

    void createSliders();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
