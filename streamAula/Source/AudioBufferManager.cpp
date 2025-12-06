/*
  ==============================================================================

    AudioBufferManager.cpp
    Implementación del gestor de buffers de audio lock-free.

  ==============================================================================
*/

#include "AudioBufferManager.h"

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
        // Buffer lleno - esto no debería pasar en condiciones normales
        // pero es mejor manejarlo que causar un crash
        return false;
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

