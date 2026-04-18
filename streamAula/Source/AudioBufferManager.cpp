/*
  ==============================================================================

    AudioBufferManager.cpp
    Implementación del gestor de buffers de audio lock-free.

  ==============================================================================
*/

#include "AudioBufferManager.h"
#include <cmath>
#include <atomic>

// Helper para crear String desde literales UTF-8 de forma segura
static inline juce::String U8(const char* utf8) 
{ 
    return juce::String(juce::CharPointer_UTF8(utf8)); 
}

// Flag para habilitar logs de diagnóstico (solo en debug)
#if JUCE_DEBUG
    #define ENABLE_BUFFER_DEBUG_LOGS 1
#else
    #define ENABLE_BUFFER_DEBUG_LOGS 0
#endif

// Contador estático para limitar frecuencia de logs
static std::atomic<int> writeLogCounter{0};
static std::atomic<int> readLogCounter{0};

//==============================================================================
AudioBufferManager::AudioBufferManager(int bufferSizeSamples, int numChannels, double sampleRate)
    : fifo(bufferSizeSamples),
      bufferSize(bufferSizeSamples),
      numChannels(numChannels),
      sampleRate(sampleRate)
{
    // Pre-reservar memoria para el buffer circular
    // Usar alineación para SIMD (JUCE lo hace automáticamente)
    circularBuffer.setSize(numChannels, bufferSizeSamples, false, false, true);
    circularBuffer.clear();
    
    // Pre-reservar buffer temporal
    tempBuffer.setSize(numChannels, bufferSizeSamples, false, false, true);
    
    // Inicializar contador de samples
    sampleCount.store(0);
}

AudioBufferManager::~AudioBufferManager()
{
}

//==============================================================================
bool AudioBufferManager::writeToBuffer(const juce::AudioBuffer<float>& input)
{
    const int numSamples = input.getNumSamples();
    const int inputChannels = input.getNumChannels();
    
    // Verificar que tenemos espacio suficiente
    if (fifo.getFreeSpace() < numSamples)
    {
        // Buffer lleno - verificar si el audio de entrada es silencio
        // Si es silencio, descartar datos antiguos para permitir escritura
        double rmsSum = 0.0;
        for (int ch = 0; ch < inputChannels; ++ch)
        {
            const float* channelData = input.getReadPointer(ch);
            double channelSum = 0.0;
            for (int i = 0; i < numSamples; ++i)
            {
                channelSum += channelData[i] * channelData[i];
            }
            double channelRms = std::sqrt(channelSum / numSamples);
            rmsSum += channelRms;
        }
        double avgRms = rmsSum / (inputChannels > 0 ? inputChannels : 1);
        
        // Si el audio de entrada es silencio y el buffer está lleno,
        // descartar datos antiguos para permitir escritura
        if (avgRms < 0.0001)
        {
            #if ENABLE_BUFFER_DEBUG_LOGS
            static int silenceWarningCount = 0;
            if (silenceWarningCount++ % 100 == 0)
            {
                juce::String warningMsg("AudioBufferManager: Buffer lleno con silencio - descartando datos antiguos. ");
                warningMsg += U8("ADVERTENCIA: El plugin no est\xc3\xa1 recibiendo audio de entrada. ");
                warningMsg += U8("Verifique la configuraci\xc3\xb3n del host.");
                juce::Logger::writeToLog(warningMsg);
            }
            #endif
            
            // Descartar datos antiguos sin alloc: tempBuffer se dimensiona en ctor/prepare al menos a bufferSize
            int samplesToDiscard = numSamples;
            jassert (tempBuffer.getNumChannels() == numChannels && tempBuffer.getNumSamples() >= samplesToDiscard);
            readFromBuffer (tempBuffer, samplesToDiscard);
            
            // Ahora debería haber espacio, intentar escribir de nuevo
            // (continuar con el código normal abajo)
        }
        else
        {
            // Audio real pero buffer lleno - NO descartar datos antiguos
            // En su lugar, perder este bloque para evitar dropeos en el stream
            // Esto es mejor que descartar datos antiguos que ya están siendo transmitidos
            #if ENABLE_BUFFER_DEBUG_LOGS
            static int overflowWarningCount = 0;
            if (overflowWarningCount++ % 100 == 0)
            {
                juce::String warningMsg("AudioBufferManager: Buffer lleno - PERDIENDO bloque actual (");
                warningMsg += juce::String(numSamples);
                warningMsg += U8(" samples). ");
                warningMsg += U8("Esto indica que el network thread no est\xc3\xa1 leyendo lo suficientemente r\xc3\xa1pido. ");
                warningMsg += U8("Aumentar velocidad de lectura o reducir tama\xc3\xb1o de chunks.");
                juce::Logger::writeToLog(warningMsg);
            }
            #endif
            
            // NO escribir este bloque - retornar false para indicar que no se escribió
            // Esto previene descartar datos antiguos que ya están siendo transmitidos
            return false;
        }
    }
    
    // Preparar escritura usando AbstractFifo (lock-free)
    int start1, size1, start2, size2;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);
    
    // Copiar datos al buffer circular
    // El buffer puede estar "envuelto" (split en dos regiones)
    if (size1 > 0)
    {
        for (int channel = 0; channel < numChannels && channel < inputChannels; ++channel)
        {
            circularBuffer.copyFrom(channel, start1, input, channel, 0, size1);
        }
    }
    
    if (size2 > 0)
    {
        for (int channel = 0; channel < numChannels && channel < inputChannels; ++channel)
        {
            circularBuffer.copyFrom(channel, start2, input, channel, size1, size2);
        }
    }
    
    // Finalizar escritura
    fifo.finishedWrite(numSamples);
    
    // Actualizar contador de samples (atómico, lock-free)
    sampleCount.fetch_add(numSamples);
    
    // Log de diagnóstico: calcular RMS del audio escrito (cada 100 escrituras)
    #if ENABLE_BUFFER_DEBUG_LOGS
    int writeCount = writeLogCounter.fetch_add(1);
    if (writeCount % 100 == 0)
    {
        double rmsSum = 0.0;
        for (int ch = 0; ch < inputChannels && ch < numChannels; ++ch)
        {
            const float* channelData = input.getReadPointer(ch);
            double channelSum = 0.0;
            for (int i = 0; i < numSamples; ++i)
            {
                channelSum += channelData[i] * channelData[i];
            }
            double channelRms = std::sqrt(channelSum / numSamples);
            rmsSum += channelRms;
        }
        double avgRms = rmsSum / (inputChannels > 0 ? inputChannels : 1);
        
        juce::String msg("AudioBufferManager: writeToBuffer - RMS: ");
        msg += juce::String(avgRms, 6);
        msg += ", samples: ";
        msg += juce::String(numSamples);
        msg += ", canales: ";
        msg += juce::String(inputChannels);
        msg += ", available: ";
        msg += juce::String(fifo.getNumReady());
        juce::Logger::writeToLog(msg);
    }
    #endif
    
    return true;
}

//==============================================================================
bool AudioBufferManager::readFromBuffer(juce::AudioBuffer<float>& output, int numSamples)
{
    // Verificar que tenemos suficientes datos
    if (fifo.getNumReady() < numSamples)
    {
        return false;
    }
    
    // Asegurar que el buffer de salida tiene el tamaño correcto
    if (output.getNumChannels() != numChannels || output.getNumSamples() < numSamples)
    {
        output.setSize(numChannels, numSamples, false, false, true);
    }
    
    // Preparar lectura usando AbstractFifo (lock-free)
    int start1, size1, start2, size2;
    fifo.prepareToRead(numSamples, start1, size1, start2, size2);
    
    // Copiar datos del buffer circular
    if (size1 > 0)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            output.copyFrom(channel, 0, circularBuffer, channel, start1, size1);
        }
    }
    
    if (size2 > 0)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            output.copyFrom(channel, size1, circularBuffer, channel, start2, size2);
        }
    }
    
    // Finalizar lectura
    fifo.finishedRead(numSamples);
    
    // Log de diagnóstico: calcular RMS del audio leído (cada 50 lecturas)
    #if ENABLE_BUFFER_DEBUG_LOGS
    int readCount = readLogCounter.fetch_add(1);
    if (readCount % 50 == 0)
    {
        double rmsSum = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* channelData = output.getReadPointer(ch);
            double channelSum = 0.0;
            for (int i = 0; i < numSamples; ++i)
            {
                channelSum += channelData[i] * channelData[i];
            }
            double channelRms = std::sqrt(channelSum / numSamples);
            rmsSum += channelRms;
        }
        double avgRms = rmsSum / numChannels;
        
        juce::String msg("AudioBufferManager: readFromBuffer - RMS: ");
        msg += juce::String(avgRms, 6);
        msg += ", samples: ";
        msg += juce::String(numSamples);
        msg += ", canales: ";
        msg += juce::String(numChannels);
        msg += ", available: ";
        msg += juce::String(fifo.getNumReady());
        juce::Logger::writeToLog(msg);
    }
    #endif
    
    return true;
}

//==============================================================================
int64_t AudioBufferManager::getCurrentSampleCount() const noexcept
{
    return sampleCount.load();
}

//==============================================================================
int AudioBufferManager::getAvailableSamples() const noexcept
{
    return fifo.getNumReady();
}

//==============================================================================
int AudioBufferManager::getFreeSpace() const noexcept
{
    return fifo.getFreeSpace();
}

//==============================================================================
void AudioBufferManager::reset()
{
    // Limpiar el buffer
    circularBuffer.clear();
    
    // Resetear el FIFO
    fifo.reset();
    
    // Resetear el contador de samples
    sampleCount.store(0);
}

//==============================================================================
void AudioBufferManager::prepare(double newSampleRate, int newNumChannels)
{
    sampleRate = newSampleRate;
    
    if (newNumChannels != numChannels)
    {
        numChannels = newNumChannels;
        
        // Re-allocar buffers con nuevo número de canales
        circularBuffer.setSize(numChannels, bufferSize, false, false, true);
        circularBuffer.clear();
        
        tempBuffer.setSize(numChannels, bufferSize, false, false, true);
    }
    
    // Resetear estado
    reset();
}

