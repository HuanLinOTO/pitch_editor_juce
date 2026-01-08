#include "PluginProcessor.h"
#include "PluginEditor.h"

PitchEditorAudioProcessor::PitchEditorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
}

PitchEditorAudioProcessor::~PitchEditorAudioProcessor() = default;

const juce::String PitchEditorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PitchEditorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PitchEditorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PitchEditorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PitchEditorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PitchEditorAudioProcessor::getNumPrograms()
{
    return 1;
}

int PitchEditorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PitchEditorAudioProcessor::setCurrentProgram (int /*index*/)
{
}

const juce::String PitchEditorAudioProcessor::getProgramName (int /*index*/)
{
    return {};
}

void PitchEditorAudioProcessor::changeProgramName (int /*index*/, const juce::String& /*newName*/)
{
}

void PitchEditorAudioProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void PitchEditorAudioProcessor::releaseResources()
{
}

#if ! JucePlugin_PreferredChannelConfigurations
bool PitchEditorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}
#endif

void PitchEditorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Current version: UI-focused plugin (audio passthrough).
}

bool PitchEditorAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PitchEditorAudioProcessor::createEditor()
{
    return new PitchEditorAudioProcessorEditor (*this);
}

void PitchEditorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    destData.reset();
}

void PitchEditorAudioProcessor::setStateInformation (const void* /*data*/, int /*sizeInBytes*/)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchEditorAudioProcessor();
}
