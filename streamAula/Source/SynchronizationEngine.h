/*
  ==============================================================================

    SynchronizationEngine.h
    Motor de sincronización sample-accurate para streaming multi-dispositivo.
    
    Implementa sincronización precisa usando timestamps basados en contador
    de samples en lugar de wall-clock time, permitiendo sincronización
    precisa entre múltiples clientes.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "AudioBufferManager.h"
#include <deque>
#include <atomic>
#include <memory>
#include <map>

//==============================================================================
/**
    Chunk de audio con información de sincronización.
*/
struct SynchronizedAudioChunk
{
    juce::AudioBuffer<float> audio;
    int64_t sampleTimestamp;      // Timestamp basado en sample count
    juce::Time wallClockTime;      // Tiempo de creación (para logging)
    int64_t sequenceNumber;        // Número de secuencia para ordenamiento
};

//==============================================================================
/**
    Motor de sincronización sample-accurate.
    
    Características:
    - Timestamps basados en contador de samples (no wall-clock)
    - Buffer de sincronización para nuevos clientes
    - Compensación adaptativa de latencia de red
    - Protocolo maestro-esclavo (servidor = maestro)
    
    El servidor mantiene un buffer de los últimos N chunks de audio
    con sus timestamps, permitiendo que nuevos clientes se sincronicen
    desde un punto conocido en el tiempo.
*/
class SynchronizationEngine
{
public:
    //==============================================================================
    /**
        Constructor.
        
        @param bufferManager    Puntero al AudioBufferManager (no es dueño)
        @param syncBufferSize   Tamaño del buffer de sincronización (número de chunks)
    */
    SynchronizationEngine(AudioBufferManager* bufferManager, int syncBufferSize = 100);
    
    /**
        Destructor.
    */
    ~SynchronizationEngine();
    
    //==============================================================================
    /**
        Obtener el timestamp actual basado en sample count.
        
        @return Timestamp actual en samples desde el inicio
    */
    int64_t getCurrentSampleTimestamp() const noexcept;
    
    /**
        Obtener un chunk de audio sincronizado para un cliente.
        
        Este método lee del AudioBufferManager y crea un chunk con
        timestamp de sincronización.
        
        @param output           Buffer de salida donde se copiará el audio
        @param numSamples       Número de samples a leer
        @param clientLatency    Latencia estimada del cliente (en samples)
        @return                 true si se obtuvo un chunk válido
    */
    bool getSynchronizedChunk(juce::AudioBuffer<float>& output, 
                               int numSamples, 
                               int64_t clientLatency = 0);
    
    /**
        Obtener información de sincronización para un nuevo cliente.
        
        Retorna el timestamp y secuencia del chunk más reciente disponible,
        permitiendo que el cliente se sincronice.
        
        @param timestamp        Timestamp del chunk más reciente
        @param sequenceNumber   Número de secuencia del chunk más reciente
        @return                 true si hay chunks disponibles
    */
    bool getSyncInfo(int64_t& timestamp, int64_t& sequenceNumber) const;
    
    /**
        Actualizar latencia estimada de un cliente.
        
        @param clientId         ID del cliente
        @param latencySamples   Latencia estimada en samples
    */
    void updateClientLatency(const juce::String& clientId, int64_t latencySamples);
    
    /**
        Resetear el contador de samples (al iniciar/reiniciar).
    */
    void reset();
    
    //==============================================================================
    /**
        Obtener información del engine.
    */
    int getSyncBufferSize() const noexcept { return syncBufferSize; }
    int getCurrentBufferSize() const;
    double getAverageLatency() const;
    
private:
    //==============================================================================
    AudioBufferManager* audioBufferManager;
    int syncBufferSize;
    
    // Buffer de sincronización (últimos N chunks)
    mutable juce::CriticalSection syncBufferLock;
    std::deque<std::unique_ptr<SynchronizedAudioChunk>> syncBuffer;
    
    // Contador de secuencia global
    std::atomic<int64_t> globalSequenceNumber{0};
    
    // Latencia de clientes (mapa thread-safe)
    mutable juce::CriticalSection latencyLock;
    std::map<juce::String, int64_t> clientLatencies;
    
    // Estadísticas
    mutable juce::CriticalSection statsLock;
    double averageLatency{0.0};
    int latencySampleCount{0};
    
    //==============================================================================
    /**
        Agregar chunk al buffer de sincronización.
    */
    void addChunkToSyncBuffer(const juce::AudioBuffer<float>& audio, int64_t timestamp);
    
    /**
        Limpiar chunks antiguos del buffer.
    */
    void cleanupOldChunks();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynchronizationEngine)
};

