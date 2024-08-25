//
// Created by Leozor on 24/08/2024.
//

#include "CrossoverProcessor.h"
void CrossoverProcessor::prepare(const juce::dsp::ProcessSpec &spec) {
    for (int i = 0; i < 4; ++i)
    {
        lowPassFilters[i]->prepare(spec);
        highPassFilters[i]->prepare(spec);
        allPassFilters[i]->prepare(spec);
    }

    updateFilters();
}

void CrossoverProcessor::process(juce::AudioBuffer<float> &buffer) {
    juce::dsp::AudioBlock<float> block(buffer);

    juce::dsp::AudioBlock<float> highBand3(block), highBand2(block), lowBand2(block);
    juce::dsp::AudioBlock<float> lowBand1(block), highBand1(block);

    highPassFilters[2]->process(juce::dsp::ProcessContextReplacing<float>(highBand3));  // HP3
    highPassFilters[1]->process(juce::dsp::ProcessContextReplacing<float>(highBand2));  // HP2 -> AP1
    allPassFilters[0]->process(juce::dsp::ProcessContextReplacing<float>(highBand2));   // AP1

    lowPassFilters[1]->process(juce::dsp::ProcessContextReplacing<float>(lowBand2));    // LP2 -> AP3
    allPassFilters[2]->process(juce::dsp::ProcessContextReplacing<float>(lowBand2));    // AP3

    allPassFilters[1]->process(juce::dsp::ProcessContextReplacing<float>(lowBand1));    // lowBand correction
    allPassFilters[3]->process(juce::dsp::ProcessContextReplacing<float>(highBand1));   // highBand correction

    // Ajout des bandes pour reconstituer le signal complet
    // Utiliser FloatVectorOperations::add pour ajouter les blocs de manière élémentaire
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        juce::FloatVectorOperations::add(buffer.getWritePointer(channel), highBand3.getChannelPointer(channel), buffer.getNumSamples());
        juce::FloatVectorOperations::add(buffer.getWritePointer(channel), highBand2.getChannelPointer(channel), buffer.getNumSamples());
        juce::FloatVectorOperations::add(buffer.getWritePointer(channel), lowBand2.getChannelPointer(channel), buffer.getNumSamples());
        juce::FloatVectorOperations::add(buffer.getWritePointer(channel), lowBand1.getChannelPointer(channel), buffer.getNumSamples());
        juce::FloatVectorOperations::add(buffer.getWritePointer(channel), highBand1.getChannelPointer(channel), buffer.getNumSamples());
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout CrossoverProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    //SUB BASS PARAMS
    params.push_back(std::make_unique<juce::AudioParameterFloat>("GAIN0", "Gain 1", -60.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("KILL0", "Kill 1", false));

    //SUB BASS -XOVER- BASS
    //scooper 18 can go from 30Hz to 200Hz
    //g-sub is efficient from 60Hz
    //g-sub can go up to 180Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>("FREQ1", "Frequency 1", 30.0f, 90.0f, 80.0f));

    //BASS PARAMS
    params.push_back(std::make_unique<juce::AudioParameterFloat>("GAIN1", "Gain 1", -60.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("KILL1", "Kill 1", false));

    //BASS -XOVER- MID
    //G-sub can go up to 180Hz
    //We use it as a kick bin, so starting at around 80Hz
    //Eminence mid can go up to 5kHz
    params.push_back(std::make_unique<juce::AudioParameterFloat>("FREQ2", "Frequency 2", 90.0f, 1500.0f, 180.0f));

    //MID PARAMS
    params.push_back(std::make_unique<juce::AudioParameterFloat>("GAIN2", "Gain 2", -60.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("KILL2", "Kill 2", false));

    //MID -XOVER- HIGH
    //Eminence mid can go up to 5kHz
    //horn can go from 1.5kHz to 20kHz
    //xover freq limits should be 1.5kHz to 5kHz
    params.push_back(std::make_unique<juce::AudioParameterFloat>("FREQ3", "Frequency 3", 1500.0f, 5000.0f, 5000.0f));

    //HIGH PARAMS
    params.push_back(std::make_unique<juce::AudioParameterFloat>("GAIN3", "Gain 3", -60.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("KILL3", "Kill 3", false));

    //HIGH -XOVER- TWEETERS
    //horn can go from 1.5kHz to 20kHz
    //tweeters can go from 4kHz to 20kHz
    //xover freq limits should be 4kHz to 20kHz
    params.push_back(std::make_unique<juce::AudioParameterFloat>("FREQ4", "Frequency 4", 4000.0f, 20000.0f, 4000.0f));

    //TWEETERS PARAMS
    params.push_back(std::make_unique<juce::AudioParameterFloat>("GAIN4", "Gain 4", -60.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("KILL4", "Kill 4", false));

    return { params.begin(), params.end() };
}
