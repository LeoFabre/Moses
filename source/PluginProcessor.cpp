#include "PluginProcessor.h"

#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor(
          BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)),
      mAPVTS(*this, nullptr, "PARAMETERS", createParameters())
{
    mAPVTS.state.addListener(this);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getMainBusNumOutputChannels();

    crossover.prepare(spec);
}

void PluginProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &)
{
    crossover.process(buffer);
}

void PluginProcessor::reset()
{
    crossover.reset();
}

juce::AudioProcessorValueTreeState& PluginProcessor::getAPVTS()
{
    return mAPVTS;
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameters()
{
    return crossover.createParameters();
}

void PluginProcessor::valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property)
{
    Listener::valueTreePropertyChanged(treeWhosePropertyHasChanged, property);
}


#pragma region Juce methods
//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void PluginProcessor::releaseResources() {}
void PluginProcessor::getStateInformation(juce::MemoryBlock &destData) {}
void PluginProcessor::setStateInformation(const void *data, int sizeInBytes) {}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
#pragma endregion
