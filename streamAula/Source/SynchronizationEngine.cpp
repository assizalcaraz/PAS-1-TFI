/*
  ==============================================================================

    SynchronizationEngine.cpp
    Implementación del motor de sincronización sample-accurate.

  ==============================================================================
*/

#include "SynchronizationEngine.h"
#include "AudioBufferManager.h"
#include <cmath>
#include <atomic>

// Flag para habilitar logs de diagnóstico (solo en debug)
#if JUCE_DEBUG
    #define ENABLE_SYNC_DEBUG_LOGS 1
#else
    #define ENABLE_SYNC_DEBUG_LOGS 0
#endif

// Contador estático para limitar frecuencia de logs
static std::atomic<int> syncLogCounter{0};

//==============================================================================
SynchronizationEngine::SynchronizationEngine(AudioBufferManager* bufferManager, int syncBufferSize)
    : audioBufferManager(bufferManager),
      syncBufferSize(syncBufferSize)
{
    jassert(bufferManager != nullptr);
    jassert(syncBufferSize > 0);
}

SynchronizationEngine::~SynchronizationEngine()
{
    const juce::ScopedLock lock(syncBufferLock);
    syncBuffer.clear();
}

//==============================================================================
int64_t SynchronizationEngine::getCurrentSampleTimestamp() const noexcept
{
    if (audioBufferManager != nullptr)
    {
        return audioBufferManager->getCurrentSampleCount();
    }
    return 0;
}

//==============================================================================
bool SynchronizationEngine::getSynchronizedChunk(juce::AudioBuffer<float>& output, 
                                                   int numSamples, 
                                                   int64_t clientLatency)
{
    if (audioBufferManager == nullptr)
        return false;
    
    // Leer audio del buffer manager
    if (!audioBufferManager->readFromBuffer(output, numSamples))
        return false;
    
    // Log de diagnóstico: calcular RMS del chunk sincronizado (cada 50 chunks)
    #if ENABLE_SYNC_DEBUG_LOGS
    int syncCount = syncLogCounter.fetch_add(1);
    if (syncCount % 50 == 0)
    {
        double rmsSum = 0.0;
        int numChannels = output.getNumChannels();
        int numSamplesInChunk = output.getNumSamples();
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* channelData = output.getReadPointer(ch);
            double channelSum = 0.0;
            for (int i = 0; i < numSamplesInChunk; ++i)
            {
                channelSum += channelData[i] * channelData[i];
            }
            double channelRms = std::sqrt(channelSum / numSamplesInChunk);
            rmsSum += channelRms;
        }
        double avgRms = rmsSum / numChannels;
        
        juce::String msg("SynchronizationEngine: getSynchronizedChunk - RMS: ");
        msg += juce::String(avgRms, 6);
        msg += ", samples: ";
        msg += juce::String(numSamplesInChunk);
        msg += ", canales: ";
        msg += juce::String(numChannels);
        msg += ", timestamp: ";
        msg += juce::String(getCurrentSampleTimestamp());
        juce::Logger::writeToLog(msg);
    }
    #endif
    
    // Obtener timestamp actual (basado en sample count)
    int64_t currentTimestamp = getCurrentSampleTimestamp();
    
    // Aplicar compensación de latencia del cliente si es necesario
    // (para futuras implementaciones de compensación adaptativa)
    int64_t adjustedTimestamp = currentTimestamp - clientLatency;
    
    // Agregar chunk al buffer de sincronización
    addChunkToSyncBuffer(output, adjustedTimestamp);
    
    return true;
}

//==============================================================================
bool SynchronizationEngine::getSyncInfo(int64_t& timestamp, int64_t& sequenceNumber) const
{
    const juce::ScopedLock lock(syncBufferLock);
    
    if (syncBuffer.empty())
    {
        // No hay chunks disponibles, usar timestamp actual
        timestamp = getCurrentSampleTimestamp();
        sequenceNumber = globalSequenceNumber.load();
        return false; // No hay chunks en buffer, pero retornamos info válida
    }
    
    // Retornar información del chunk más reciente
    const auto& latestChunk = syncBuffer.back();
    timestamp = latestChunk->sampleTimestamp;
    sequenceNumber = latestChunk->sequenceNumber;
    
    return true;
}

//==============================================================================
void SynchronizationEngine::updateClientLatency(const juce::String& clientId, int64_t latencySamples)
{
    const juce::ScopedLock lock(latencyLock);
    
    clientLatencies[clientId] = latencySamples;
    
    // Actualizar promedio de latencia
    {
        const juce::ScopedLock statsLock(this->statsLock);
        double totalLatency = averageLatency * latencySampleCount + latencySamples;
        latencySampleCount++;
        averageLatency = totalLatency / latencySampleCount;
    }
}

//==============================================================================
void SynchronizationEngine::reset()
{
    const juce::ScopedLock lock(syncBufferLock);
    
    syncBuffer.clear();
    globalSequenceNumber.store(0);
    
    {
        const juce::ScopedLock latencyLock(this->latencyLock);
        clientLatencies.clear();
    }
    
    {
        const juce::ScopedLock statsLock(this->statsLock);
        averageLatency = 0.0;
        latencySampleCount = 0;
    }
}

//==============================================================================
int SynchronizationEngine::getCurrentBufferSize() const
{
    const juce::ScopedLock lock(syncBufferLock);
    return (int)syncBuffer.size();
}

//==============================================================================
double SynchronizationEngine::getAverageLatency() const
{
    const juce::ScopedLock lock(statsLock);
    return averageLatency;
}

//==============================================================================
void SynchronizationEngine::addChunkToSyncBuffer(const juce::AudioBuffer<float>& audio, int64_t timestamp)
{
    const juce::ScopedLock lock(syncBufferLock);
    
    // Crear nuevo chunk sincronizado
    auto chunk = std::make_unique<SynchronizedAudioChunk>();
    chunk->audio.makeCopyOf(audio);
    chunk->sampleTimestamp = timestamp;
    chunk->wallClockTime = juce::Time::getCurrentTime();
    chunk->sequenceNumber = globalSequenceNumber.fetch_add(1);
    
    // Agregar al buffer
    syncBuffer.push_back(std::move(chunk));
    
    // Limpiar chunks antiguos si el buffer está lleno
    if ((int)syncBuffer.size() > syncBufferSize)
    {
        cleanupOldChunks();
    }
}

//==============================================================================
void SynchronizationEngine::cleanupOldChunks()
{
    // Mantener solo los últimos syncBufferSize chunks
    while ((int)syncBuffer.size() > syncBufferSize)
    {
        syncBuffer.pop_front();
    }
}

