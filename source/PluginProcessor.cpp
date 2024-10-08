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

#include "PluginProcessor.h"

#include "FilterVisualizerHelper.h"
#include "PluginEditor.h"

//==============================================================================
MultiBandCompressorAudioProcessor::MultiBandCompressorAudioProcessor() :
    AudioProcessorBase (
#ifndef JucePlugin_PreferredChannelConfigurations
        BusesProperties()
    #if ! JucePlugin_IsMidiEffect
        #if ! JucePlugin_IsSynth
            .withInput ("Input", juce::AudioChannelSet::discreteChannels (64), true)
        #endif
            .withOutput ("Output", juce::AudioChannelSet::discreteChannels (64), true)
    #endif
            ,
#endif
        createParameterLayout()),
    maxNumFilters (ceil (64 / IIRfloat_elements))
{
    const juce::String inputSettingID = "orderSetting";
    orderSetting = parameters.getRawParameterValue (inputSettingID);
    parameters.addParameterListener (inputSettingID, this);

    for (int filterBandIdx = 0; filterBandIdx < numFilterBands - 1; ++filterBandIdx)
    {
        const juce::String crossoverID ("crossover" + juce::String (filterBandIdx));

        crossovers[filterBandIdx] = parameters.getRawParameterValue (crossoverID);

        lowPassLRCoeffs[filterBandIdx] =
            IIR::Coefficients<double>::makeLowPass (lastSampleRate, *crossovers[filterBandIdx]);
        highPassLRCoeffs[filterBandIdx] =
            IIR::Coefficients<double>::makeHighPass (lastSampleRate, *crossovers[filterBandIdx]);

        calculateCoefficients (filterBandIdx);

        iirLPCoefficients[filterBandIdx] =
            IIR::Coefficients<float>::makeLowPass (lastSampleRate, *crossovers[filterBandIdx]);
        iirHPCoefficients[filterBandIdx] =
            IIR::Coefficients<float>::makeHighPass (lastSampleRate, *crossovers[filterBandIdx]);
        iirAPCoefficients[filterBandIdx] =
            IIR::Coefficients<float>::makeAllPass (lastSampleRate, *crossovers[filterBandIdx]);

        parameters.addParameterListener (crossoverID, this);

        iirLP[filterBandIdx].clear();
        iirLP2[filterBandIdx].clear();
        iirHP[filterBandIdx].clear();
        iirHP2[filterBandIdx].clear();
        iirAP[filterBandIdx].clear();

        for (int simdFilterIdx = 0; simdFilterIdx < maxNumFilters; ++simdFilterIdx)
        {
            iirLP[filterBandIdx].add (new IIR::Filter<IIRfloat> (iirLPCoefficients[filterBandIdx]));
            iirLP2[filterBandIdx].add (
                new IIR::Filter<IIRfloat> (iirLPCoefficients[filterBandIdx]));
            iirHP[filterBandIdx].add (new IIR::Filter<IIRfloat> (iirHPCoefficients[filterBandIdx]));
            iirHP2[filterBandIdx].add (
                new IIR::Filter<IIRfloat> (iirHPCoefficients[filterBandIdx]));
            iirAP[filterBandIdx].add (new IIR::Filter<IIRfloat> (iirAPCoefficients[filterBandIdx]));
        }
    }


    for (int filterBandIdx = 0; filterBandIdx < numFilterBands; ++filterBandIdx)
    {
        for (int simdFilterIdx = 0; simdFilterIdx < maxNumFilters; ++simdFilterIdx)
        {
            freqBandsBlocks[filterBandIdx].push_back (juce::HeapBlock<char>());
        }

        const juce::String soloID ("solo" + juce::String (filterBandIdx));
        const juce::String killID ("kill" + juce::String (filterBandIdx));
        const juce::String gainID ("gain" + juce::String (filterBandIdx));

        gain[filterBandIdx] = parameters.getRawParameterValue(gainID);

        parameters.addParameterListener (soloID, this);
        parameters.addParameterListener (killID, this);
        parameters.addParameterListener(gainID, this);
    }

    soloArray.clear();
    killArray.clear();

    copyCoeffsToProcessor();

    for (int simdFilterIdx = 0; simdFilterIdx < maxNumFilters; ++simdFilterIdx)
    {
        interleavedBlockData.push_back (juce::HeapBlock<char>());
    }
}

MultiBandCompressorAudioProcessor::~MultiBandCompressorAudioProcessor()
{
    inputAnalyser.stopThread (1000);
    outputAnalyser.stopThread (1000);
}

std::vector<std::unique_ptr<juce::RangedAudioParameter>>
    MultiBandCompressorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    const float crossoverPresets[numFilterBands - 1] = { 80.0f, 440.0f, 2200.0f, 5000.0f };

    // Ambisonics Order
    auto floatParam = std::make_unique<juce::AudioParameterFloat> (
        "orderSetting",
        "Ambisonics Order",
        juce::NormalisableRange<float> (0.0f, 8.0f, 1.0f),
        0.0f,
        "",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int maximumStringLength)
        {
            if (value >= 0.5f && value < 1.5f)
                return "0th";
            else if (value >= 1.5f && value < 2.5f)
                return "1st";
            else if (value >= 2.5f && value < 3.5f)
                return "2nd";
            else if (value >= 3.5f && value < 4.5f)
                return "3rd";
            else if (value >= 4.5f && value < 5.5f)
                return "4th";
            else if (value >= 5.5f && value < 6.5f)
                return "5th";
            else if (value >= 6.5f && value < 7.5f)
                return "6th";
            else if (value >= 7.5f)
                return "7th";
            else
                return "Auto";
        },
        nullptr);
    params.push_back (std::move (floatParam));

    // Normalization
    floatParam = std::make_unique<juce::AudioParameterFloat> (
        "useSN3D",
        "Normalization",
        juce::NormalisableRange<float> (0.0f, 1.0f, 1.0f),
        1.0f,
        "",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int maximumStringLength)
        {
            if (value >= 0.5f)
                return "SN3D";
            else
                return "N3D";
        },
        nullptr);
    params.push_back (std::move (floatParam));

    // Crossovers
    for (int i = 0; i < numFilterBands - 1; ++i)
    {
        floatParam = std::make_unique<juce::AudioParameterFloat> (
            "crossover" + juce::String (i),
            "Crossover " + juce::String (i),
            juce::NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.4f),
            crossoverPresets[i],
            "Hz",
            juce::AudioProcessorParameter::genericParameter,
            std::function<juce::String (float value, int maximumStringLength)> (
                [] (float v, int m) { return juce::String (v, m); }),
            std::function<float (const juce::String& text)> ([] (const juce::String& t)
                                                             { return t.getFloatValue(); }));
        params.push_back (std::move (floatParam));
    }

    //Band gain
    for (int i = 0; i < numFilterBands; ++i)
    {
        floatParam = std::make_unique<juce::AudioParameterFloat>(
            "gain" + juce::String(i),
            "Gain " + juce::String(i),
            juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f),
            0.0f, "dB");
        params.push_back(std::move(floatParam));
    }


    for (int i = 0; i < numFilterBands; ++i)
    {
        auto boolParam = std::make_unique<juce::AudioParameterBool> ("solo" + juce::String (i),
                                                                "Solo band " + juce::String (i),
                                                                false);
        params.push_back (std::move (boolParam));
        boolParam = std::make_unique<juce::AudioParameterBool> ("kill" + juce::String (i),
                                                                "Kill band " + juce::String (i),
                                                                false);
        params.push_back (std::move (boolParam));
    }

    return params;
}

void MultiBandCompressorAudioProcessor::calculateCoefficients (const int i)
{
    jassert (lastSampleRate > 0.0);

    const float crossoverFrequency =
        juce::jmin (static_cast<float> (0.5 * lastSampleRate), crossovers[i]->load());

    double b0, b1, b2, a0, a1, a2;
    double K = std::tan (juce::MathConstants<double>::pi * (crossoverFrequency) / lastSampleRate);
    double den = 1 + juce::MathConstants<double>::sqrt2 * K + pow (K, double (2.0));

    // calculate coeffs for 2nd order Butterworth
    a0 = 1.0;
    a1 = (2 * (pow (K, 2.0) - 1)) / den;
    a2 = (1 - juce::MathConstants<double>::sqrt2 * K + pow (K, 2.0)) / den;

    // HP
    b0 = 1.0 / den;
    b1 = -2.0 * b0;
    b2 = b0;
    iirTempHPCoefficients[i] = new IIR::Coefficients<float> (b0, b1, b2, a0, a1, a2);

    // also calculate 4th order Linkwitz-Riley for GUI
    IIR::Coefficients<double>::Ptr coeffs (new IIR::Coefficients<double> (b0, b1, b2, a0, a1, a2));
    coeffs->coefficients =
        FilterVisualizerHelper<double>::cascadeSecondOrderCoefficients (coeffs->coefficients,
                                                                        coeffs->coefficients);
    *highPassLRCoeffs[i] = *coeffs;

    // LP
    b0 = pow (K, 2.0) / den;
    b1 = 2.0 * b0;
    b2 = b0;
    iirTempLPCoefficients[i] = new IIR::Coefficients<float> (b0, b1, b2, a0, a1, a2);

    coeffs.reset();
    coeffs = (new IIR::Coefficients<double> (b0, b1, b2, a0, a1, a2));
    coeffs->coefficients =
        FilterVisualizerHelper<double>::cascadeSecondOrderCoefficients (coeffs->coefficients,
                                                                        coeffs->coefficients);
    *lowPassLRCoeffs[i] = *coeffs;

    // Allpass equivalent to 4th order Linkwitz-Riley crossover
    iirTempAPCoefficients[i] = new IIR::Coefficients<float> (a2, a1, a0, a0, a1, a2);
}

void MultiBandCompressorAudioProcessor::copyCoeffsToProcessor()
{
    for (int filterBandIdx = 0; filterBandIdx < numFilterBands - 1; ++filterBandIdx)
    {
        *iirLPCoefficients[filterBandIdx] = *iirTempLPCoefficients[filterBandIdx]; // LP
        *iirHPCoefficients[filterBandIdx] = *iirTempHPCoefficients[filterBandIdx]; // HP
        *iirAPCoefficients[filterBandIdx] = *iirTempAPCoefficients[filterBandIdx]; // AP
    }

    userChangedFilterSettings = false;
}

//==============================================================================
int MultiBandCompressorAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
        // so this should be at least 1, even if you're not really implementing programs.
}

int MultiBandCompressorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MultiBandCompressorAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MultiBandCompressorAudioProcessor::getProgramName (int index)
{
    return {};
}

void MultiBandCompressorAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MultiBandCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // checkInputAndOutput (this, *orderSetting, *orderSetting, true);

    lastSampleRate = sampleRate;

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = samplesPerBlock;
    monoSpec.numChannels = 1;

    inputPeak = juce::Decibels::gainToDecibels (-INFINITY);
    outputPeak = juce::Decibels::gainToDecibels (-INFINITY);

    for (int filterBandIdx = 0; filterBandIdx < numFilterBands - 1; ++filterBandIdx)
    {
        calculateCoefficients (filterBandIdx);
    }

    copyCoeffsToProcessor();

    interleavedData.clear();
    for (int simdFilterIdx = 0; simdFilterIdx < maxNumFilters; ++simdFilterIdx)
    {
        interleavedData.add (
            new juce::dsp::AudioBlock<IIRfloat> (interleavedBlockData[simdFilterIdx],
                                                 1,
                                                 samplesPerBlock));
        clear (*interleavedData.getLast());
    }

    for (int filterBandIdx = 0; filterBandIdx < numFilterBands - 1; ++filterBandIdx)
    {
        for (int simdFilterIdx = 0; simdFilterIdx < maxNumFilters; ++simdFilterIdx)
        {
            iirLP[filterBandIdx][simdFilterIdx]->reset (IIRfloat (0.0f));
            iirLP2[filterBandIdx][simdFilterIdx]->reset (IIRfloat (0.0f));
            iirHP[filterBandIdx][simdFilterIdx]->reset (IIRfloat (0.0f));
            iirHP2[filterBandIdx][simdFilterIdx]->reset (IIRfloat (0.0f));
            iirAP[filterBandIdx][simdFilterIdx]->reset (IIRfloat (0.0f));
            iirLP[filterBandIdx][simdFilterIdx]->prepare (monoSpec);
            iirLP2[filterBandIdx][simdFilterIdx]->prepare (monoSpec);
            iirHP[filterBandIdx][simdFilterIdx]->prepare (monoSpec);
            iirHP2[filterBandIdx][simdFilterIdx]->prepare (monoSpec);
            iirAP[filterBandIdx][simdFilterIdx]->prepare (monoSpec);
        }
    }

    for (int filterBandIdx = 0; filterBandIdx < numFilterBands; ++filterBandIdx)
    {
        freqBands[filterBandIdx].clear();
        for (int simdFilterIdx = 0; simdFilterIdx < maxNumFilters; ++simdFilterIdx)
        {
            freqBands[filterBandIdx].add (
                new juce::dsp::AudioBlock<IIRfloat> (freqBandsBlocks[filterBandIdx][simdFilterIdx],
                                                     1,
                                                     samplesPerBlock));
        }
    }

    zero = juce::dsp::AudioBlock<float> (zeroData, IIRfloat_elements, samplesPerBlock);
    zero.clear();

    gains = juce::dsp::AudioBlock<float> (gainData, 1, samplesPerBlock);
    gains.clear();

    tempBuffer.setSize (64, samplesPerBlock, false, true);

    inputAnalyser.setupAnalyser  (int (sampleRate), float (sampleRate));
    outputAnalyser.setupAnalyser (int (sampleRate), float (sampleRate));

    repaintFilterVisualization = true;
}

void MultiBandCompressorAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    inputAnalyser.stopThread (1000);
    outputAnalyser.stopThread (1000);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MultiBandCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return true;
}
#endif

void MultiBandCompressorAudioProcessor::processBlock (juce::AudioSampleBuffer& buffer,
                                                      juce::MidiBuffer& midiMessages)
{
    // checkInputAndOutput (this, *orderSetting, *orderSetting, false);
    juce::ScopedNoDenormals noDenormals;

    const int maxNChIn = buffer.getNumChannels();
    if (maxNChIn < 1)
        return;

    if (getActiveEditor() != nullptr)
        inputAnalyser.addAudioData (buffer, 0, getTotalNumInputChannels());

    const int L = buffer.getNumSamples();
    const int nSIMDFilters = 1 + (maxNChIn - 1) / IIRfloat_elements;
    gainChannelPointer = gains.getChannelPointer(0);

    gains.clear();
    zero.clear();

    // update iir filter coefficients
    if (userChangedFilterSettings.get())
        copyCoeffsToProcessor();

    inputPeak = juce::Decibels::gainToDecibels(buffer.getMagnitude(0, 0, L));

    using Format = juce::AudioData::Format<juce::AudioData::Float32, juce::AudioData::NativeEndian>;

    // Interleave input data
    int partial = maxNChIn % IIRfloat_elements;
    if (partial == 0)
    {
        for (int simdFilterIdx = 0; simdFilterIdx < nSIMDFilters; ++simdFilterIdx)
        {
            juce::AudioData::interleaveSamples(
                juce::AudioData::NonInterleavedSource<Format> {
                    buffer.getArrayOfReadPointers() + simdFilterIdx * IIRfloat_elements,
                    IIRfloat_elements },
                juce::AudioData::InterleavedDest<Format> {
                    reinterpret_cast<float*>(
                        interleavedData[simdFilterIdx]->getChannelPointer(0)),
                    IIRfloat_elements },
                L);
        }
    }
    else
    {
        int simdFilterIdx;
        for (simdFilterIdx = 0; simdFilterIdx < nSIMDFilters - 1; ++simdFilterIdx)
        {
            juce::AudioData::interleaveSamples(
                juce::AudioData::NonInterleavedSource<Format> {
                    buffer.getArrayOfReadPointers() + simdFilterIdx * IIRfloat_elements,
                    IIRfloat_elements },
                juce::AudioData::InterleavedDest<Format> {
                    reinterpret_cast<float*>(
                        interleavedData[simdFilterIdx]->getChannelPointer(0)),
                    IIRfloat_elements },
                L);
        }

        const float* addr[IIRfloat_elements];
        int iirElementIdx;
        for (iirElementIdx = 0; iirElementIdx < partial; ++iirElementIdx)
        {
            addr[iirElementIdx] =
                buffer.getReadPointer(simdFilterIdx * IIRfloat_elements + iirElementIdx);
        }
        for (; iirElementIdx < IIRfloat_elements; ++iirElementIdx)
        {
            addr[iirElementIdx] = zero.getChannelPointer(iirElementIdx);
        }
        juce::AudioData::interleaveSamples(
            juce::AudioData::NonInterleavedSource<Format> { addr, IIRfloat_elements },
            juce::AudioData::InterleavedDest<Format> {
                reinterpret_cast<float*>(interleavedData[simdFilterIdx]->getChannelPointer(0)),
                IIRfloat_elements },
            L);
    }

    //  filter block diagram
    //                                | ---> HP3 ---> |
    //        | ---> HP2 ---> AP1 --->|               + ---> |
    //        |                       | ---> LP3 ---> |      |
    //     -->|                                              + --->
    //        |                       | ---> HP1 ---> |      |
    //        | ---> LP2 ---> AP3 --->|               + ---> |
    //                                | ---> LP1 ---> |
    for (int simdFilterIdx = 0; simdFilterIdx < nSIMDFilters; ++simdFilterIdx)
    {
        const IIRfloat* chPtrinterleavedData[1] = {
            interleavedData[simdFilterIdx]->getChannelPointer(0)
        };
        juce::dsp::AudioBlock<IIRfloat> abInterleaved(
            const_cast<IIRfloat**>(chPtrinterleavedData),
            1,
            L);

        const IIRfloat* chPtrLow[1] = {
            freqBands[FrequencyBands::Low][simdFilterIdx]->getChannelPointer(0)
        };
        juce::dsp::AudioBlock<IIRfloat> abLow(const_cast<IIRfloat**>(chPtrLow), 1, L);

        const IIRfloat* chPtrMidLow[1] = {
            freqBands[FrequencyBands::MidLow][simdFilterIdx]->getChannelPointer(0)
        };
        juce::dsp::AudioBlock<IIRfloat> abMidLow(const_cast<IIRfloat**>(chPtrMidLow), 1, L);

        const IIRfloat* chPtrMid[1] = {
            freqBands[FrequencyBands::Mid][simdFilterIdx]->getChannelPointer(0)
        };
        juce::dsp::AudioBlock<IIRfloat> abMid(const_cast<IIRfloat**>(chPtrMid), 1, L);

        const IIRfloat* chPtrMidHigh[1] = {
            freqBands[FrequencyBands::MidHigh][simdFilterIdx]->getChannelPointer(0)
        };
        juce::dsp::AudioBlock<IIRfloat> abMidHigh(const_cast<IIRfloat**>(chPtrMidHigh), 1, L);

        const IIRfloat* chPtrHigh[1] = {
            freqBands[FrequencyBands::High][simdFilterIdx]->getChannelPointer(0)
        };
        juce::dsp::AudioBlock<IIRfloat> abHigh(const_cast<IIRfloat**>(chPtrHigh), 1, L);


        // Traitement de la bande Low
        iirLP[1][simdFilterIdx]->process(
            juce::dsp::ProcessContextNonReplacing<IIRfloat>(abInterleaved, abLow));
        iirHP[1][simdFilterIdx]->process(
            juce::dsp::ProcessContextNonReplacing<IIRfloat>(abInterleaved, abHigh));

        iirLP2[1][simdFilterIdx]->process(juce::dsp::ProcessContextReplacing<IIRfloat>(abLow));
        iirHP2[1][simdFilterIdx]->process(juce::dsp::ProcessContextReplacing<IIRfloat>(abHigh));

        // Traitement de la bande MidLow
        iirAP[2][simdFilterIdx]->process(juce::dsp::ProcessContextReplacing<IIRfloat>(abLow));
        iirAP[0][simdFilterIdx]->process(juce::dsp::ProcessContextReplacing<IIRfloat>(abHigh));

        iirHP[0][simdFilterIdx]->process(
            juce::dsp::ProcessContextNonReplacing<IIRfloat>(abLow, abMidLow));
        iirHP2[0][simdFilterIdx]->process(juce::dsp::ProcessContextReplacing<IIRfloat>(abMidLow));

        iirLP[0][simdFilterIdx]->process(juce::dsp::ProcessContextReplacing<IIRfloat>(abLow));
        iirLP2[0][simdFilterIdx]->process(juce::dsp::ProcessContextReplacing<IIRfloat>(abLow));

        // Traitement de la bande Mid (nouvelle bande)
        iirLP[2][simdFilterIdx]->process(
            juce::dsp::ProcessContextNonReplacing<IIRfloat>(abMidLow, abMid));
        iirHP[2][simdFilterIdx]->process(
            juce::dsp::ProcessContextNonReplacing<IIRfloat>(abMidLow, abMidHigh));

        iirLP2[2][simdFilterIdx]->process(juce::dsp::ProcessContextReplacing<IIRfloat>(abMid));
        iirHP2[2][simdFilterIdx]->process(juce::dsp::ProcessContextReplacing<IIRfloat>(abMidHigh));

        // Traitement de la bande MidHigh
        iirLP[3][simdFilterIdx]->process(
            juce::dsp::ProcessContextNonReplacing<IIRfloat>(abHigh, abMidHigh));
        iirLP2[3][simdFilterIdx]->process(
            juce::dsp::ProcessContextReplacing<IIRfloat>(abMidHigh));

        iirHP[3][simdFilterIdx]->process(juce::dsp::ProcessContextReplacing<IIRfloat>(abHigh));
        iirHP2[3][simdFilterIdx]->process(juce::dsp::ProcessContextReplacing<IIRfloat>(abHigh));

    }

    buffer.clear();

    for (int filterBandIdx = 0; filterBandIdx < numFilterBands; ++filterBandIdx)
    {
        // Vérifier si la bande est en mode Kill
        if (killArray[filterBandIdx])
        {
            continue;  // Si c'est le cas, ne pas l'ajouter au mix final
        }
        if (!soloArray.isZero())
        {
            if (!soloArray[filterBandIdx])
            {
                continue;
            }
        }

        tempBuffer.clear();

        // Deinterleave
        if (partial == 0)
        {
            for (int simdFilterIdx = 0; simdFilterIdx < nSIMDFilters; ++simdFilterIdx)
            {
                juce::AudioData::deinterleaveSamples(
                    juce::AudioData::InterleavedSource<Format> {
                        reinterpret_cast<float*>(
                            freqBands[filterBandIdx][simdFilterIdx]->getChannelPointer(0)),
                        IIRfloat_elements },
                    juce::AudioData::NonInterleavedDest<Format> {
                        tempBuffer.getArrayOfWritePointers() + simdFilterIdx * IIRfloat_elements,
                        IIRfloat_elements },
                    L);
            }
        }
        else
        {
            int simdFilterIdx;
            for (simdFilterIdx = 0; simdFilterIdx < nSIMDFilters - 1; ++simdFilterIdx)
            {
                juce::AudioData::deinterleaveSamples(
                    juce::AudioData::InterleavedSource<Format> {
                        reinterpret_cast<float*>(
                            freqBands[filterBandIdx][simdFilterIdx]->getChannelPointer(0)),
                        IIRfloat_elements },
                    juce::AudioData::NonInterleavedDest<Format> {
                        tempBuffer.getArrayOfWritePointers() + simdFilterIdx * IIRfloat_elements,
                        IIRfloat_elements },
                    L);
            }

            float* addr[IIRfloat_elements];
            int iirElementIdx;
            for (iirElementIdx = 0; iirElementIdx < partial; ++iirElementIdx)
            {
                addr[iirElementIdx] =
                    tempBuffer.getWritePointer(simdFilterIdx * IIRfloat_elements + iirElementIdx);
            }
            for (; iirElementIdx < IIRfloat_elements; ++iirElementIdx)
            {
                addr[iirElementIdx] = zero.getChannelPointer(iirElementIdx);
            }
            juce::AudioData::deinterleaveSamples(
                juce::AudioData::InterleavedSource<Format> {
                    reinterpret_cast<float*>(
                        freqBands[filterBandIdx][simdFilterIdx]->getChannelPointer(0)),
                    IIRfloat_elements },
                juce::AudioData::NonInterleavedDest<Format> { addr, IIRfloat_elements },
                L);
            zero.clear();
        }

        // Apply band gain
        float bandGain = juce::Decibels::decibelsToGain(gain[filterBandIdx]->load());
        for (int ch = 0; ch < maxNChIn; ++ch)
        {
            juce::FloatVectorOperations::multiply(tempBuffer.getWritePointer(ch), bandGain, L);
            juce::FloatVectorOperations::add(buffer.getWritePointer(ch),
                                             tempBuffer.getReadPointer(ch),
                                             L);
            maxPeak[filterBandIdx] = juce::Decibels::gainToDecibels(
                                                    tempBuffer.getMagnitude(ch, 0, L));
        }
    }


    if (getActiveEditor() != nullptr)
        outputAnalyser.addAudioData (buffer, 0, getTotalNumOutputChannels());
    outputPeak = juce::Decibels::gainToDecibels(buffer.getMagnitude(0, 0, L));
}

void MultiBandCompressorAudioProcessor::createAnalyserPlot (juce::Path& p, const juce::Rectangle<int> bounds, float minFreq, bool input)
{
    if (input)
        inputAnalyser.createPath (p, bounds.toFloat(), minFreq);
    else
        outputAnalyser.createPath (p, bounds.toFloat(), minFreq);
}

bool MultiBandCompressorAudioProcessor::checkForNewAnalyserData()
{
    return inputAnalyser.checkForNewData() || outputAnalyser.checkForNewData();
}

//==============================================================================
bool MultiBandCompressorAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MultiBandCompressorAudioProcessor::createEditor()
{
    return new MultiBandCompressorAudioProcessorEditor (*this, parameters);
}

//==============================================================================
void MultiBandCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();

    auto oscConfig = state.getOrCreateChildWithName ("OSCConfig", nullptr);
    oscConfig.copyPropertiesFrom (oscParameterInterface.getConfig(), nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MultiBandCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
            if (parameters.state.hasProperty ("OSCPort")) // legacy
            {
                oscParameterInterface.getOSCReceiver().connect (
                    parameters.state.getProperty ("OSCPort", juce::var (-1)));
                parameters.state.removeProperty ("OSCPort", nullptr);
            }

            auto oscConfig = parameters.state.getChildWithName ("OSCConfig");
            if (oscConfig.isValid())
                oscParameterInterface.setConfig (oscConfig);
        }
}

//==============================================================================
void MultiBandCompressorAudioProcessor::parameterChanged (const juce::String& parameterID,
                                                          float newValue)
{
    DBG ("Parameter with ID " << parameterID << " has changed. New value: " << newValue);

    if (parameterID.startsWith ("crossover"))
    {
        calculateCoefficients (parameterID.getLastCharacters (1).getIntValue());
        userChangedFilterSettings = true;
        repaintFilterVisualization = true;
    }
    else if (parameterID.startsWith ("solo"))
    {
        if (newValue >= 0.5f)
            soloArray.setBit (parameterID.getLastCharacters (1).getIntValue());
        else
            soloArray.clearBit (parameterID.getLastCharacters (1).getIntValue());
    }
    else if (parameterID.startsWith("kill"))
    {
        if (newValue >= 0.5f)
            killArray.setBit(parameterID.getLastCharacters(1).getIntValue());
        else
            killArray.clearBit(parameterID.getLastCharacters(1).getIntValue());
    }
    else if (parameterID.startsWith ("gain"))
    {
        const int bandId = parameterID.getLastCharacters (1).getIntValue();
        gain[bandId]->store(newValue);
        repaintFilterVisualization = true;
    }
    else
    {
        DBG ("Parameter with ID " << parameterID << " has changed. New value: " << newValue);
    }
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultiBandCompressorAudioProcessor();
}

inline void MultiBandCompressorAudioProcessor::clear (juce::dsp::AudioBlock<IIRfloat>& ab)
{
    const int N = static_cast<int> (ab.getNumSamples()) * IIRfloat_elements;
    const int nCh = static_cast<int> (ab.getNumChannels());

    for (int ch = 0; ch < nCh; ++ch)
        juce::FloatVectorOperations::clear (reinterpret_cast<float*> (ab.getChannelPointer (ch)),
                                            N);
}
