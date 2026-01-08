#include "PluginEditor.h"

PitchEditorAudioProcessorEditor::PitchEditorAudioProcessorEditor (PitchEditorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    addAndMakeVisible (mainComponent);
    setSize (1400, 900);
}

PitchEditorAudioProcessorEditor::~PitchEditorAudioProcessorEditor() = default;

void PitchEditorAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void PitchEditorAudioProcessorEditor::resized()
{
    mainComponent.setBounds (getLocalBounds());
}
