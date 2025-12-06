/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "AudioBufferManager.h"
#include "NetworkStreamer.h"

//==============================================================================
/**
*/
class StreamAulaAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    StreamAulaAudioProcessor();
    ~StreamAulaAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

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
    // Métodos públicos para acceso desde el Editor
    AudioBufferManager* getAudioBufferManager() const noexcept { return audioBufferManager.get(); }
    bool isBufferActive() const noexcept { return audioBufferManager != nullptr; }
    NetworkStreamer* getNetworkStreamer() const noexcept { return networkStreamer.get(); }
    bool isServerActive() const noexcept { return networkStreamer != nullptr && networkStreamer->isServerActive(); }

private:
    //==============================================================================
    // AudioBufferManager para gestión lock-free de buffers
    // Buffer circular: ~1 segundo de audio a 44.1kHz (44100 samples)
    static constexpr int BUFFER_SIZE_SAMPLES = 44100;
    
    std::unique_ptr<AudioBufferManager> audioBufferManager;
    std::unique_ptr<NetworkStreamer> networkStreamer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StreamAulaAudioProcessor)
};
