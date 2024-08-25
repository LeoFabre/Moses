#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

class CrossoverProcessor
{
public:
    CrossoverProcessor()
    {
        for (int i = 0; i < 4; ++i)
        {
            lowPassFilters[i] = std::make_unique<juce::dsp::LinkwitzRileyFilter<float>>();
            lowPassFilters[i]->setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
            highPassFilters[i] = std::make_unique<juce::dsp::LinkwitzRileyFilter<float> >();
            highPassFilters[i]->setType(juce::dsp::LinkwitzRileyFilterType::highpass);
            allPassFilters[i] = std::make_unique<juce::dsp::IIR::Filter<float> >();
        }
    }

    void prepare(const juce::dsp::ProcessSpec& spec);

    void updateFilters()
    {
        float crossoverFrequencies[4] = { 200.0f, 800.0f, 3000.0f, 8000.0f };

        for (int i = 0; i < 4; ++i)
        {
            lowPassFilters[i]->setCutoffFrequency(crossoverFrequencies[i]);
            highPassFilters[i]->setCutoffFrequency(crossoverFrequencies[i]);
            allPassFilters[i]->coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(44100.0, crossoverFrequencies[i]);
        }
    }

    void process(juce::AudioBuffer<float>& buffer);

    void reset()
    {
        for (int i = 0; i < 4; ++i)
        {
            lowPassFilters[i]->reset();
            highPassFilters[i]->reset();
            allPassFilters[i]->reset();
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

private:
    std::unique_ptr<juce::dsp::LinkwitzRileyFilter<float>> lowPassFilters[4];
    std::unique_ptr<juce::dsp::LinkwitzRileyFilter<float>> highPassFilters[4];
    std::unique_ptr<juce::dsp::IIR::Filter<float>> allPassFilters[4];
};