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

#include <juce_audio_processors/juce_audio_processors.h>

#include "Analyser.h"
#include "juce_dsp/juce_dsp.h"
#include "AudioProcessorBase.h"

#define ProcessorClass MultiBandCompressorAudioProcessor
#define numFilterBands 5

using namespace juce::dsp;
using ParameterLayout = juce::AudioProcessorValueTreeState::ParameterLayout;

class MultiBandCompressorAudioProcessor
    : public AudioProcessorBase
{
public:
    constexpr static int numberOfInputChannels = 2;
    constexpr static int numberOfOutputChannels = 2;
    //==============================================================================
    MultiBandCompressorAudioProcessor();
    ~MultiBandCompressorAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
#endif

    void processBlock (juce::AudioSampleBuffer&, juce::MidiBuffer&) override;

    void createAnalyserPlot(juce::Path &p, juce::Rectangle<int> bounds, float minFreq, bool input);

    bool checkForNewAnalyserData();

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    //======= Parameters ===========================================================
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameterLayout();

#if JUCE_USE_SIMD
    using IIRfloat = SIMDRegister<float>;
    static constexpr int IIRfloat_elements = SIMDRegister<float>::size();
#else /* !JUCE_USE_SIMD */
    using IIRfloat = float;
    static constexpr int IIRfloat_elements = 1;
#endif

    enum FrequencyBands
    {
        Low,
        MidLow,
        Mid,
        MidHigh,
        High
    };

    // Interface for gui
    double& getSampleRate() { return lastSampleRate; };
    IIR::Coefficients<double>::Ptr lowPassLRCoeffs[numFilterBands - 1];
    IIR::Coefficients<double>::Ptr highPassLRCoeffs[numFilterBands - 1];

    juce::Atomic<bool> repaintFilterVisualization = false;
    juce::Atomic<float> inputPeak = juce::Decibels::gainToDecibels (-INFINITY),
                        outputPeak = juce::Decibels::gainToDecibels (-INFINITY);
    juce::Atomic<float> maxGR[numFilterBands], maxPeak[numFilterBands];

    juce::Atomic<bool> characteristicHasChanged[numFilterBands];


    //analysers
    Analyser<float> inputAnalyser;
    Analyser<float> outputAnalyser;

private:
    void calculateCoefficients (int index);
    void copyCoeffsToProcessor();

    inline void clear (AudioBlock<IIRfloat>& ab);

    double lastSampleRate { 48000 };
    const int maxNumFilters;

    // list of used audio parameters
    std::atomic<float>* orderSetting;
    std::atomic<float>* crossovers[numFilterBands - 1];
    std::atomic<float>* gain[numFilterBands];

    juce::BigInteger soloArray;
    juce::BigInteger killArray;

    // filter coefficients
    juce::dsp::IIR::Coefficients<float>::Ptr iirLPCoefficients[numFilterBands - 1],
        iirHPCoefficients[numFilterBands - 1], iirAPCoefficients[numFilterBands - 1],
        iirTempLPCoefficients[numFilterBands - 1], iirTempHPCoefficients[numFilterBands - 1],
        iirTempAPCoefficients[numFilterBands - 1];

    // filters (cascaded butterworth/linkwitz-riley filters + allpass)
    juce::OwnedArray<IIR::Filter<IIRfloat>> iirLP[numFilterBands - 1], iirHP[numFilterBands - 1],
        iirLP2[numFilterBands - 1], iirHP2[numFilterBands - 1], iirAP[numFilterBands - 1];

    // data for interleaving audio
    juce::HeapBlock<char> zeroData;
    std::vector<juce::HeapBlock<char>> interleavedBlockData;
    juce::OwnedArray<juce::dsp::AudioBlock<IIRfloat>> interleavedData;
    juce::dsp::AudioBlock<float> zero;
    juce::AudioBuffer<float> tempBuffer;

    // filters for processing
    juce::OwnedArray<juce::dsp::AudioBlock<IIRfloat>> freqBands[numFilterBands];
    std::vector<juce::HeapBlock<char>> freqBandsBlocks[numFilterBands];

    // Additional compressor parameters
    float* gainChannelPointer;
    juce::dsp::AudioBlock<float> gains;
    juce::HeapBlock<char> gainData;

    juce::Atomic<bool> userChangedFilterSettings = true;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiBandCompressorAudioProcessor)
};
