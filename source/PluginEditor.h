/*
*   MultiBandComp Plugin
*
*   Made by Jacob Curtis
*   Using JUCE Framework
*   Tested on Windows 10 using Reaper in VST3 format
*
*/

#pragma once
#include "PluginProcessor.h"
#include "../../Modules/GUI-Components.h"
#include "../../Modules/Meters.h"

class MultiBandCompAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    MultiBandCompAudioProcessorEditor(MultiBandCompAudioProcessor &);

    ~MultiBandCompAudioProcessorEditor() override;

    void paint(juce::Graphics &) override {}

    void resized() override;

private:
    MultiBandCompAudioProcessor &audioProcessor;

    BgImage bgImage;
    PowerLine powerLine{"Multiband Comp", "Jacob Curtis", 30};
    std::array<std::unique_ptr<MultiLabel>, numBands> bandLabels;
    std::array<std::unique_ptr<GainReductionMeter>, numBands> grMeters;
    std::array<std::unique_ptr<LevelMeter>, numBands> levelMeters;
    std::array<std::unique_ptr<SmallKnob>, numBands - 1> freqKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, numBands - 1> freqAttach;
    std::array<std::unique_ptr<OuterKnob>, numBands> ratioKnobs{
        std::make_unique<OuterKnob>(": 1"),
        std::make_unique<OuterKnob>(": 1"),
        std::make_unique<OuterKnob>(": 1"),
        std::make_unique<OuterKnob>(": 1"),
        };
    std::array<std::unique_ptr<SmallKnob>, numBands> thresholdKnobs{
        std::make_unique<SmallKnob>("", "dB"),
        std::make_unique<SmallKnob>("", "dB"),
        std::make_unique<SmallKnob>("", "dB"),
        std::make_unique<SmallKnob>("", "dB")
    };
    std::array<std::unique_ptr<SmallKnob>, numBands> attackKnobs{
        std::make_unique<SmallKnob>("Attack", "ms"),
        std::make_unique<SmallKnob>("Attack", "ms"),
        std::make_unique<SmallKnob>("Attack", "ms"),
        std::make_unique<SmallKnob>("Attack", "ms")
    };
    std::array<std::unique_ptr<SmallKnob>, numBands> releaseKnobs{
        std::make_unique<SmallKnob>("Release", "ms"),
        std::make_unique<SmallKnob>("Release", "ms"),
        std::make_unique<SmallKnob>("Release", "ms"),
        std::make_unique<SmallKnob>("Release", "ms")
    };
    std::array<std::unique_ptr<SmallKnob>, numBands> makeUpKnobs{
        std::make_unique<SmallKnob>("Gain", "dB"),
        std::make_unique<SmallKnob>("Gain", "dB"),
        std::make_unique<SmallKnob>("Gain", "dB"),
        std::make_unique<SmallKnob>("Gain", "dB")
    };
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, numBands> thresholdAttach,
            ratioAttach, attackAttach, releaseAttach, makeUpAttach;
    SmallButton stereoButton{"Stereo"};
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> stereoAttach;
    MultiLabel listenLabel{"Listen"};
    MultiLabel killLabel{"Kill"};
    std::array<SmallButton, numBands> listenButtons;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, numBands> listenAttach;

    std::array<SmallButton, numBands> killButtons;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>, numBands> killAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiBandCompAudioProcessorEditor)
};
