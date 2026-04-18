/*
  ==============================================================================

    AudioBufferManager.h
    Gestión eficiente de buffers de audio con operaciones lock-free.
    
    Implementa un buffer circular usando juce::AbstractFifo para permitir
    escritura desde el audio thread y lectura desde el network thread sin locks.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>

//==============================================================================
/**
    Gestor de buffers de audio lock-free para streaming.
    
    Permite escribir desde el audio thread (processBlock) y leer desde
    un único hilo consumidor (fan-out de red) sin locks en el FIFO,
    usando juce::AbstractFifo (contrato SPSC: un escritor, un lector).
    
    Características:
    - Buffer circular lock-free
    - Pre-reserva de memoria
    - Alineación para SIMD
    - Contador de samples atómico
*/
class AudioBufferManager
{
public:
    //==============================================================================
    /**
        Constructor.
        
        @param bufferSizeSamples    Tamaño del buffer circular en samples
        @param numChannels          Número de canales de audio (1=mono, 2=stereo)
        @param sampleRate           Tasa de muestreo (para cálculos de tiempo)
    */
    AudioBufferManager(int bufferSizeSamples, int numChannels, double sampleRate = 44100.0);
    
    /**
        Destructor.
    */
    ~AudioBufferManager();
    
    //==============================================================================
    /**
        Escribir audio al buffer circular desde el audio thread.
        
        Esta función es lock-free y debe ser llamada solo desde processBlock().
        
        @param input    Buffer de audio de entrada
        @return         true si se escribió correctamente, false si el buffer estaba lleno
    */
    bool writeToBuffer(const juce::AudioBuffer<float>& input);
    
    /**
        Leer audio del buffer circular desde el network thread.
        
        Esta función es lock-free y puede ser llamada desde cualquier thread
        que no sea el audio thread.
        
        @param output       Buffer de salida donde se copiará el audio
        @param numSamples   Número de samples a leer
        @return             true si se leyó correctamente, false si no hay suficientes datos
    */
    bool readFromBuffer(juce::AudioBuffer<float>& output, int numSamples);
    
    /**
        Obtener el contador de samples actual.
        
        @return Número total de samples procesados desde el inicio
    */
    int64_t getCurrentSampleCount() const noexcept;
    
    /**
        Obtener el número de samples disponibles para lectura.
        
        @return Número de samples disponibles en el buffer
    */
    int getAvailableSamples() const noexcept;
    
    /**
        Obtener el número de samples que se pueden escribir sin bloqueo.
        
        @return Espacio disponible en el buffer
    */
    int getFreeSpace() const noexcept;
    
    /**
        Resetear el buffer y el contador de samples.
    */
    void reset();
    
    /**
        Preparar el buffer para un nuevo sample rate o número de canales.
        
        @param sampleRate   Nueva tasa de muestreo
        @param numChannels  Nuevo número de canales
    */
    void prepare(double sampleRate, int numChannels);
    
    //==============================================================================
    /**
        Obtener información del buffer.
    */
    int getBufferSize() const noexcept { return bufferSize; }
    int getNumChannels() const noexcept { return numChannels; }
    double getSampleRate() const noexcept { return sampleRate; }
    
private:
    //==============================================================================
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> circularBuffer;
    
    int bufferSize;
    int numChannels;
    double sampleRate;
    
    // Contador atómico de samples procesados
    std::atomic<int64_t> sampleCount{0};
    
    // Buffer temporal para operaciones de escritura/lectura
    juce::AudioBuffer<float> tempBuffer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioBufferManager)
};

