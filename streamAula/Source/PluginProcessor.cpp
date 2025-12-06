/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "NetworkStreamer.h"

//==============================================================================
StreamAulaAudioProcessor::StreamAulaAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

StreamAulaAudioProcessor::~StreamAulaAudioProcessor()
{
}

//==============================================================================
const juce::String StreamAulaAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool StreamAulaAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool StreamAulaAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool StreamAulaAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double StreamAulaAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int StreamAulaAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int StreamAulaAudioProcessor::getCurrentProgram()
{
    return 0;
}

void StreamAulaAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String StreamAulaAudioProcessor::getProgramName (int index)
{
    return {};
}

void StreamAulaAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void StreamAulaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Inicializar AudioBufferManager con el sample rate y número de canales
    // Buffer circular de ~1 segundo de audio
    int numChannels = getTotalNumInputChannels();
    if (numChannels == 0)
        numChannels = 2; // Default a estéreo si no hay input
    
    audioBufferManager = std::make_unique<AudioBufferManager>(
        BUFFER_SIZE_SAMPLES,
        numChannels,
        sampleRate
    );
    
    // Inicializar NetworkStreamer
    networkStreamer = std::make_unique<NetworkStreamer>(
        audioBufferManager.get(),
        8080  // Puerto por defecto
    );
    
    // Iniciar servidor
    if (!networkStreamer->startServer())
    {
        // Error al iniciar servidor (puede ser que el puerto esté ocupado)
        networkStreamer.reset();
    }
}

void StreamAulaAudioProcessor::releaseResources()
{
    // Detener y liberar NetworkStreamer
    if (networkStreamer != nullptr)
    {
        networkStreamer->stopServer();
        networkStreamer.reset();
    }
    
    // Liberar AudioBufferManager cuando se detiene la reproducción
    audioBufferManager.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool StreamAulaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void StreamAulaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Passthrough: el audio pasa sin modificación
    // Además, escribimos al buffer circular para streaming
    
    // Escribir al buffer circular si está inicializado
    if (audioBufferManager != nullptr)
    {
        // Escribir audio al buffer circular (lock-free, desde audio thread)
        audioBufferManager->writeToBuffer(buffer);
    }
    
    // El audio ya está en el buffer de salida (passthrough)
    // No necesitamos hacer nada más aquí
}

//==============================================================================
bool StreamAulaAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* StreamAulaAudioProcessor::createEditor()
{
    return new StreamAulaAudioProcessorEditor (*this);
}

//==============================================================================
void StreamAulaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void StreamAulaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StreamAulaAudioProcessor();
}
