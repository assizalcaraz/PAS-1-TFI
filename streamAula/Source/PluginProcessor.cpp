/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "NetworkStreamer.h"
#include <cmath>
#include <atomic>

// Helper para crear String desde literales UTF-8 de forma segura
// Evita problemas de aserción en juce_String.cpp con caracteres no ASCII
static inline juce::String U8(const char* utf8) 
{ 
    return juce::String(juce::CharPointer_UTF8(utf8)); 
}

// Flag para habilitar logs de diagnóstico (solo en debug)
#if JUCE_DEBUG
    #define ENABLE_PROCESSBLOCK_DEBUG_LOGS 1
#else
    #define ENABLE_PROCESSBLOCK_DEBUG_LOGS 0
#endif

// Contador estático para limitar frecuencia de logs
static std::atomic<int> processBlockLogCounter{0};

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
    // Log de configuración de audio
    #if JUCE_DEBUG
    {
        int numInputChannels = getTotalNumInputChannels();
        int numOutputChannels = getTotalNumOutputChannels();
        auto inputLayout = getBusesLayout().getMainInputChannelSet();
        auto outputLayout = getBusesLayout().getMainOutputChannelSet();
        
        juce::String configMsg(U8("PluginProcessor: prepareToPlay - SampleRate: "));
        configMsg += juce::String(sampleRate, 0);
        configMsg += U8(" Hz, SamplesPerBlock: ");
        configMsg += juce::String(samplesPerBlock);
        configMsg += U8(", Input Channels: ");
        configMsg += juce::String(numInputChannels);
        configMsg += U8(", Output Channels: ");
        configMsg += juce::String(numOutputChannels);
        configMsg += U8(", Input Layout: ");
        configMsg += inputLayout.getDescription();
        configMsg += U8(", Output Layout: ");
        configMsg += outputLayout.getDescription();
        
        if (numInputChannels == 0)
        {
            configMsg += U8(" [ADVERTENCIA: No hay canales de entrada configurados!]");
            configMsg += U8("\n  -> En modo standalone, configure el dispositivo de entrada en Audio/MIDI Settings");
            configMsg += U8("\n  -> Verifique que el dispositivo de entrada este activo y no silenciado");
        }
        else
        {
            configMsg += U8("\n  -> Canales de entrada configurados correctamente");
            configMsg += U8("\n  -> Si no hay audio, verifique permisos de microfono en macOS");
            configMsg += U8("\n  -> Verifique que el dispositivo de entrada este activo en Audio/MIDI Settings");
        }
        
        juce::Logger::writeToLog(configMsg);
    }
    #endif
    
    // Inicializar AudioBufferManager con el sample rate y número de canales
    // Buffer circular de ~1 segundo de audio
    int numChannels = getTotalNumInputChannels();
    if (numChannels == 0)
    {
        numChannels = 2; // Default a estéreo si no hay input
        #if JUCE_DEBUG
        juce::Logger::writeToLog(U8("PluginProcessor: WARNING - No hay canales de entrada, usando default est\xc3\xa9reo (2 canales)"));
        #endif
    }
    
    audioBufferManager = std::make_unique<AudioBufferManager>(
        BUFFER_SIZE_SAMPLES,
        numChannels,
        sampleRate
    );
    
    // Inicializar SynchronizationEngine
    synchronizationEngine = std::make_unique<SynchronizationEngine>(
        audioBufferManager.get(),
        100  // Buffer de sincronización: últimos 100 chunks
    );
    
    // Inicializar NetworkStreamer con SynchronizationEngine
    // IMPORTANTE: Verificar que no exista ya un NetworkStreamer activo
    // para evitar reinicios en bucle cuando se recrea el editor o cambia el estado
    if (networkStreamer != nullptr && networkStreamer->isServerActive())
    {
        // Ya existe un servidor activo, no reiniciar
        // Esto previene reinicios cuando prepareToPlay se llama múltiples veces
        return;
    }
    
    // Si existe pero no está activo, liberarlo primero
    if (networkStreamer != nullptr)
    {
        networkStreamer->stopServer();
        networkStreamer.reset();
    }
    
    networkStreamer = std::make_unique<NetworkStreamer>(
        audioBufferManager.get(),
        synchronizationEngine.get(),  // Pasar SynchronizationEngine
        8080  // Puerto por defecto
    );
    
    // Iniciar servidor
    #if JUCE_DEBUG
    juce::String logMsg("PluginProcessor: Intentando iniciar NetworkStreamer en puerto 8080...");
    juce::Logger::writeToLog(logMsg);
    #endif
    
    if (!networkStreamer->startServer())
    {
        // Error al iniciar servidor (puede ser que el puerto esté ocupado)
        #if JUCE_DEBUG
        juce::String errorMsg = U8("PluginProcessor: ERROR - No se pudo iniciar NetworkStreamer. El puerto puede estar ocupado.");
        juce::Logger::writeToLog(errorMsg);
        #endif
        networkStreamer.reset();
    }
    #if JUCE_DEBUG
    else
    {
        juce::String successMsg = U8("PluginProcessor: NetworkStreamer iniciado correctamente.");
        juce::Logger::writeToLog(successMsg);
    }
    #endif
}

void StreamAulaAudioProcessor::releaseResources()
{
    // Detener y liberar NetworkStreamer
    if (networkStreamer != nullptr)
    {
        networkStreamer->stopServer();
        networkStreamer.reset();
    }
    
    // Liberar SynchronizationEngine
    if (synchronizationEngine != nullptr)
    {
        synchronizationEngine->reset();
        synchronizationEngine.reset();
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
    
    // DIAGNÓSTICO: Verificar si el buffer de entrada tiene datos reales
    // En modo standalone, si getTotalNumInputChannels() > 0 pero el buffer está en silencio,
    // significa que el dispositivo de entrada está configurado pero no está enviando datos
    // (puede estar silenciado, nivel en 0, o sin permisos)

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Log de diagnóstico: calcular RMS del buffer de entrada (cada 100 bloques)
    #if ENABLE_PROCESSBLOCK_DEBUG_LOGS
    int blockCount = processBlockLogCounter.fetch_add(1);
    if (blockCount % 100 == 0)
    {
        double rmsSum = 0.0;
        double maxAbsValue = 0.0;  // Valor máximo absoluto para detectar si hay señal
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            const float* channelData = buffer.getReadPointer(ch);
            double channelSum = 0.0;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                double absValue = std::abs(channelData[i]);
                if (absValue > maxAbsValue)
                    maxAbsValue = absValue;
                channelSum += channelData[i] * channelData[i];
            }
            double channelRms = std::sqrt(channelSum / buffer.getNumSamples());
            rmsSum += channelRms;
        }
        double avgRms = totalNumInputChannels > 0 ? (rmsSum / totalNumInputChannels) : 0.0;
        
        juce::String rmsMsg(U8("PluginProcessor: processBlock - RMS entrada: "));
        rmsMsg += juce::String(avgRms, 6);
        rmsMsg += U8(", Max abs: ");
        rmsMsg += juce::String(maxAbsValue, 6);
        rmsMsg += U8(", samples: ");
        rmsMsg += juce::String(buffer.getNumSamples());
        rmsMsg += U8(", canales entrada: ");
        rmsMsg += juce::String(totalNumInputChannels);
        rmsMsg += U8(", audioBufferManager: ");
        rmsMsg += (audioBufferManager != nullptr ? "SI" : "NO");
        
        // Verificar si todos los valores son exactamente cero
        bool allZeros = (maxAbsValue < 0.0000001);
        
        // Advertencia si el RMS es muy bajo (posible silencio)
        if (avgRms < 0.0001 && totalNumInputChannels > 0)
        {
            rmsMsg += U8(" [ADVERTENCIA: No hay audio de entrada]");
            rmsMsg += U8(", todos ceros: ");
            rmsMsg += (allZeros ? "SI" : "NO");
            
            if (blockCount % 500 == 0) // Mostrar instrucciones cada 500 bloques
            {
                rmsMsg += U8("\n  -> Verifique permisos de microfono en macOS");
                rmsMsg += U8("\n  -> Verifique que el dispositivo de entrada este activo en Audio/MIDI Settings");
                rmsMsg += U8("\n  -> Verifique que el microfono no este silenciado en macOS");
                if (allZeros) {
                    rmsMsg += U8("\n  -> El buffer esta completamente en silencio (todos los valores son 0.0)");
                    rmsMsg += U8("\n  -> Esto indica que el host no esta enviando datos de entrada");
                }
            }
        }
        
        juce::Logger::writeToLog(rmsMsg);
    }
    #endif

    // Passthrough: el audio pasa sin modificación
    // Además, escribimos al buffer circular para streaming
    
    // Escribir al buffer circular si está inicializado
    // IMPORTANTE: Escribir siempre, incluso si no hay audio (silencio)
    // Esto permite que el stream funcione y el cliente pueda conectarse
    if (audioBufferManager != nullptr)
    {
        // Escribir audio al buffer circular (lock-free, desde audio thread)
        // Escribimos siempre, incluso si es silencio, para mantener el stream activo
        bool written = audioBufferManager->writeToBuffer(buffer);
        
        #if ENABLE_PROCESSBLOCK_DEBUG_LOGS
        if (blockCount % 100 == 0)
        {
            int available = audioBufferManager->getAvailableSamples();
            juce::String writeMsg(U8("PluginProcessor: writeToBuffer retorn\xc3\xb3: "));
            writeMsg += (written ? "true" : "false");
            writeMsg += U8(", availableSamples: ");
            writeMsg += juce::String(available);
            juce::Logger::writeToLog(writeMsg);
        }
        #endif
    }
    else
    {
        #if ENABLE_PROCESSBLOCK_DEBUG_LOGS
        if (blockCount % 100 == 0)
        {
            juce::Logger::writeToLog(U8("PluginProcessor: WARNING - audioBufferManager es nullptr!"));
        }
        #endif
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
