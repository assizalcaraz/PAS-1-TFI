/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class StreamAulaAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                        public juce::Timer
{
public:
    StreamAulaAudioProcessorEditor (StreamAulaAudioProcessor&);
    ~StreamAulaAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    
    //==============================================================================
    // Timer para actualizar la UI periódicamente
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    StreamAulaAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StreamAulaAudioProcessorEditor)
};
