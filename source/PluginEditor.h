/*
 ==============================================================================
 This file is part of the IEM plug-in suite.
 Author: Markus Huber
 Copyright (c) 2017 - Institute of Electronic Music and Acoustics (IEM)
 https://iem.at

 The IEM plug-in suite is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 The IEM plug-in suite is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this software.  If not, see <https://www.gnu.org/licenses/>.
 ==============================================================================
 */

#pragma once

#include "PluginProcessor.h"

#include "lookAndFeel/IEM_LaF.h"
//Plugin Design Essentials
#include "Components/TitleBar.h"

//Custom juce::Components
#include "Components/FilterVisualizer.h"
#include "Components/LevelMeter.h"
#include "Components/ReverseSlider.h"
#include "Components/RoundButton.h"
#include "Components/SimpleLabel.h"
#include "FilterBankVisualizer.h"

using SliderAttachment = ReverseSlider::
    SliderAttachment; // all ReverseSliders will make use of the parameters' valueToText() function
using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

//==============================================================================
/**
*/
class MultiBandCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                private juce::Timer,
                                                public juce::Slider::Listener,
                                                public juce::Button::Listener
{
public:
    MultiBandCompressorAudioProcessorEditor (MultiBandCompressorAudioProcessor&,
                                             juce::AudioProcessorValueTreeState&);
    ~MultiBandCompressorAudioProcessorEditor();

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void sliderValueChanged (juce::Slider* slider) override;
    void buttonClicked (juce::Button* bypassButton) override;

    void timerCallback() override;

private:
    // ====================== begin essentials ==================
    // lookAndFeel class with the IEM plug-in suite design
    LaF globalLaF;

    // stored references to the AudioProcessor and juce::ValueTreeState holding all the parameters
    MultiBandCompressorAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& valueTreeState;

    /* title and footer component
     title component can hold different widgets for in- and output:
        - NoIOWidget (if there's no need for an input or output widget)
        - AudioChannelsIOWidget<maxNumberOfChannels, isChoosable>
        - AmbisonicIOWidget<maxOrder>
        - DirectivitiyIOWidget
     */
    TitleBar<AmbisonicIOWidget<>, NoIOWidget> title;
    OSCFooter footer;
    // =============== end essentials ============

    std::unique_ptr<ComboBoxAttachment> cbNormalizationAtachement;
    std::unique_ptr<ComboBoxAttachment> cbOrderAtachement;

    FilterBankVisualizer<double> filterBankVisualizer;
    juce::TooltipWindow tooltips;

    // Filter Crossovers
    ReverseSlider slCrossover[numFilterBands - 1];
    std::unique_ptr<SliderAttachment> slCrossoverAttachment[numFilterBands - 1];

    // Solo and Bypass juce::Buttons
    RoundButton tbSolo[numFilterBands];
    RoundButton tbBypass[numFilterBands];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        soloAttachment[numFilterBands], bypassAttachment[numFilterBands];

    // Band gain sliders
    ReverseSlider slBandGain[numFilterBands];
    std::unique_ptr<SliderAttachment> slBandGainAttachment[numFilterBands];
    LevelMeter bandLevelMeters[numFilterBands];

    LevelMeter omniInputMeter, omniOutputMeter;

    // juce::Toggle juce::Buttons
    juce::ToggleButton tbOverallMagnitude;
    bool displayOverallMagnitude { false };

    // juce::Labels
    SimpleLabel lbKnee[numFilterBands + 1], lbThreshold[numFilterBands + 1],
        lbMakeUpGain[numFilterBands + 1], lbRatio[numFilterBands + 1], lbAttack[numFilterBands + 1],
        lbRelease[numFilterBands + 1], lbInput, lbOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiBandCompressorAudioProcessorEditor)
};
