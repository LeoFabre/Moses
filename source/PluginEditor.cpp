/*
*   MultiBandComp Plugin
*
*   Made by Jacob Curtis
*   Using JUCE Framework
*   Tested on Windows 10 using Reaper in VST3 format
*
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

MultiBandCompAudioProcessorEditor::MultiBandCompAudioProcessorEditor(MultiBandCompAudioProcessor &p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      grMeters{
          std::make_unique<GainReductionMeter>(audioProcessor.gainReduction[0]),
          std::make_unique<GainReductionMeter>(audioProcessor.gainReduction[1]),
          std::make_unique<GainReductionMeter>(audioProcessor.gainReduction[2]),
          std::make_unique<GainReductionMeter>(audioProcessor.gainReduction[3])
      },
      freqKnobs{
          std::make_unique<SmallKnob>("Freq", "Hz"),
          std::make_unique<SmallKnob>("Freq", "Hz"),
          std::make_unique<SmallKnob>("Freq", "Hz")
      },
      bandLabels{
          std::make_unique<MultiLabel>("Band 1"),
          std::make_unique<MultiLabel>("Band 2"),
          std::make_unique<MultiLabel>("Band 3"),
          std::make_unique<MultiLabel>("Band 4"),
      }
{
    addAndMakeVisible(bgImage);
    addAndMakeVisible(powerLine);
    addAndMakeVisible(listenLabel);
    addAndMakeVisible(killLabel);
    // frequency controls
    for (int crossover = 0; crossover < numBands - 1; crossover++) {
        addAndMakeVisible(freqKnobs[crossover].get());
    }
    freqAttach[0] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "crossoverFreqB", *freqKnobs[0]);
    freqAttach[1] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "crossoverFreqA", *freqKnobs[1]);
    freqAttach[2] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "crossoverFreqC", *freqKnobs[2]);
    // compression controls
    for (int band = 0; band < numBands; band++) {
        const auto bandNum = juce::String(band + 1);
        addAndMakeVisible(bandLabels[band].get());
        addAndMakeVisible(grMeters[band].get());
        addAndMakeVisible(ratioKnobs[band].get());
        addAndMakeVisible(thresholdKnobs[band].get());
        addAndMakeVisible(attackKnobs[band].get());
        addAndMakeVisible(releaseKnobs[band].get());
        addAndMakeVisible(makeUpKnobs[band].get());
        addAndMakeVisible(listenButtons[band]);
        addAndMakeVisible(killButtons[band]);
        thresholdAttach[band] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, "threshold" + bandNum, *thresholdKnobs[band]);
        ratioAttach[band] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, "ratio" + bandNum, *ratioKnobs[band]);
        attackAttach[band] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, "attack" + bandNum, *attackKnobs[band]);
        releaseAttach[band] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, "release" + bandNum, *releaseKnobs[band]);
        makeUpAttach[band] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, "makeUp" + bandNum, *makeUpKnobs[band]);
        listenAttach[band] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.parameters, "listen" + bandNum, listenButtons[band]);
        killAttach[band] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.parameters, "kill" + bandNum, killButtons[band]);
    }
    addAndMakeVisible(stereoButton);
    stereoAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "stereo", stereoButton);
    setSize(730, 560);
}

MultiBandCompAudioProcessorEditor::~MultiBandCompAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void MultiBandCompAudioProcessorEditor::resized()
{
    bgImage.setBounds(getLocalBounds());
    powerLine.setBounds(0, 10, 300, 50);
    for (int band = 0; band < numBands; band++) {
        ratioKnobs[band].get()->setBounds(band * 170 + 40, 170, 80, 120);
        thresholdKnobs[band].get()->setBounds(ratioKnobs[band].get()->getInnerArea());
        attackKnobs[band].get()->setBounds(ratioKnobs[band].get()->getX() - 10,
                                           ratioKnobs[band].get()->getBottom() - 25, 40, 70);
        releaseKnobs[band].get()->setBounds(attackKnobs[band].get()->getRight() + 20, attackKnobs[band].get()->getY(),
                                            40, 70);
        makeUpKnobs[band].get()->setBounds(attackKnobs[band].get()->getX() + 30, attackKnobs[band].get()->getBottom(),
                                           40, 70);
        grMeters[band].get()->setBounds(ratioKnobs[band].get()->getRight() + 20, ratioKnobs[band].get()->getY() - 10,
                                        grMeters[band].get()->getMeterWidth(), grMeters[band].get()->getMeterHeight());
        bandLabels[band].get()->setBounds(ratioKnobs[band].get()->getX() - 10, ratioKnobs[band].get()->getY() - 30, 160,
                                          15);
        listenButtons[band].setBounds(makeUpKnobs[band].get()->getX() - 5, makeUpKnobs[band].get()->getBottom() + 30,
                                      50, 50);
        killButtons[band].setBounds(listenButtons[band].getX(), listenButtons[band].getBottom() + 30, 50, 50);
    }
    for (int crossover = 0; crossover < numBands - 1; crossover++) {
        freqKnobs[crossover].get()->setBounds(ratioKnobs[crossover + 1].get()->getX() - 40, 60, 50, 80);
    }
    stereoButton.setBounds(freqKnobs[2].get()->getRight() + 50, freqKnobs[2].get()->getY() + 15, 50, 60);
    const int xPos = listenButtons[0].getX() + (listenButtons[0].getWidth() / 2) - 1;
    const int width = listenButtons[3].getX() + (listenButtons[3].getWidth() / 2) - xPos + 1;
    listenLabel.setBounds(xPos, listenButtons[0].getY() - 20, width, 13);
    killLabel.setBounds(xPos, killButtons[0].getY() - 20, width, 13);
}
