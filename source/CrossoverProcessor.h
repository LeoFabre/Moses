#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

class CrossoverProcessor
{
public:
    CrossoverProcessor();

    void prepare(const juce::dsp::ProcessSpec& spec);

    void updateFilters() const;

    void process(juce::AudioBuffer<float>& buffer);

    void reset() const;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    size_t getNumChannelsPrepared() const { return numChannelsPrepared; }

private:
    std::unique_ptr<juce::dsp::LinkwitzRileyFilter<float>> lowPassFilters[4];
    std::unique_ptr<juce::dsp::LinkwitzRileyFilter<float>> highPassFilters[4];
    std::unique_ptr<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>> allPassFilters[4];
    size_t numChannelsPrepared = 0;

};