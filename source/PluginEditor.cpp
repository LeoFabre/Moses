#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginEditor::PluginEditor(PluginProcessor &p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{

    createSliders();
    setSize(800, 300);
}


PluginEditor::~PluginEditor() = default;

//==============================================================================
void PluginEditor::paint(juce::Graphics &g) {
    g.fillAll(juce::Colours::black);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().removeFromBottom(getHeight() / 2);
    int sliderWidth = area.getWidth() / (numBands + (numBands - 1));

    for (int i = 0; i < numBands; ++i)
    {
        gainSliders[i]->setBounds(area.removeFromLeft(sliderWidth));
        if (i < numBands - 1)
        {
            freqSliders[i]->setBounds(area.removeFromLeft(sliderWidth));
        }
    }
}

void PluginEditor::createSliders()
{
    // Ajouter les sliders de gain pour chaque bande
    for (int i = 0; i < numBands; ++i)
    {
        auto* gainSlider = new GainSlider(processorRef, "GAIN" + juce::String(i));
        gainSliders.add(gainSlider);
        addAndMakeVisible(gainSlider);
    }

    // Ajouter les sliders de fréquence pour chaque transition entre les bandes
    for (int i = 1; i < numBands; ++i)  // Il y a un slider de fréquence de moins que de bandes
    {
        auto* freqSlider = new FreqSlider(processorRef, "FREQ" + juce::String(i));
        freqSliders.add(freqSlider);
        addAndMakeVisible(freqSlider);
    }
}