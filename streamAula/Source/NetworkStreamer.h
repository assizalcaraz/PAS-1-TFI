/*
  ==============================================================================

    NetworkStreamer.h
    Servidor HTTP embebido para streaming de audio a múltiples clientes.
    
    Implementa un servidor HTTP que lee del AudioBufferManager (un solo lector
    vía hilo de fan-out) y envía audio a múltiples clientes conectados
    simultáneamente.

  ==============================================================================
*/

#pragma once

#include <cstdint>
#include <JuceHeader.h>
#include "AudioBufferManager.h"
#include "SynchronizationEngine.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

class NetworkStreamer;

//==============================================================================
/**
    Cola PCM interleaved (int16) por cliente. El hilo de broadcast empuja copias;
    el ClientWorker consume hacia el socket. Fuera del audio thread.
*/
class PerClientAudioQueue
{
public:
    static constexpr int maxQueuedChunks = 64;

    void pushMove (std::vector<int16_t>&& interleavedPcm);

    /** @return false si timeout o shutdown */
    bool popChunk (std::vector<int16_t>& out, int timeoutMs);

    void signalShutdown() noexcept;
    bool isShutdown() const noexcept { return shutdown.load (std::memory_order_acquire); }

private:
    mutable std::mutex mtx;
    std::condition_variable cv;
    std::deque<std::vector<int16_t>> chunks;
    std::atomic<bool> shutdown { false };
};

//==============================================================================
/**
    Cliente conectado al servidor de streaming.
    (Mantenido para compatibilidad de API pública)
*/
struct StreamingClient
{
    juce::String clientId;
    juce::Time connectionTime;
    int64_t lastSampleSent{0};
    bool isActive{true};
};

//==============================================================================
/**
    Worker thread que maneja una conexión de cliente individual.
    
    Cada ClientWorker ejecuta en su propio hilo y maneja:
    - Lectura del request HTTP
    - Procesamiento de endpoints
    - Para /stream: consume PCM desde PerClientAudioQueue (alimentada por el
      hilo de broadcast único lector del AudioBufferManager)
*/
class ClientWorker : public juce::Thread
{
public:
    ClientWorker (juce::StreamingSocket* clientSocket,
                  AudioBufferManager* bufferManager,
                  SynchronizationEngine* syncEngine,
                  NetworkStreamer* serverOwner);

    ~ClientWorker() override;

    void run() override;

    StreamingClient getClientInfo() const;

    /** true mientras el hilo del worker sigue en ejecución (válido entre hilos). */
    bool isClientActive() const noexcept { return isThreadRunning(); }

    void stopStreaming();

private:
    std::unique_ptr<juce::StreamingSocket> clientSocket;
    AudioBufferManager* audioBufferManager = nullptr;
    SynchronizationEngine* synchronizationEngine = nullptr;
    NetworkStreamer* ownerStreamer = nullptr;

    std::atomic<bool> shouldStop{false};

    juce::String clientId;
    juce::Time connectionTime;
    int64_t lastSampleSent{0};

    static constexpr bool ENABLE_DEBUG_LOGS =
    #if JUCE_DEBUG
        true
    #else
        false
    #endif
    ;

    void processHttpRequest (const juce::String& request);

    /** @return false si el socket ya no puede enviar (cliente desconectado o error de escritura). */
    bool sendInt16InterleavedChunk (const int16_t* data, int numInterleavedSamples, int numChannels);

    void debugLog (const juce::String& message) const;

    static bool isValidHttpText (const void* data, int size);

    static constexpr int maxChunkSamples = 512;
    static constexpr int maxChannelsSupported = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClientWorker)
};

//==============================================================================
/**
    Servidor HTTP para streaming de audio.
    
    - Un hilo acepta conexiones (NetworkStreamer::run).
    - Un hilo AudioBroadcast lee una vez por tick del AudioBufferManager y
      replica PCM a colas por cliente (fan-out).
    - Cada ClientWorker solo hace I/O HTTP/socket para /stream.
*/
class NetworkStreamer : public juce::Thread
{
public:
    NetworkStreamer (AudioBufferManager* bufferManager,
                     SynchronizationEngine* syncEngine = nullptr,
                     int port = 8080);

    ~NetworkStreamer() override;

    bool startServer();

    void stopServer();

    bool isServerActive() const noexcept { return serverActive.load(); }

    int getPort() const noexcept { return serverPort; }

    int getNumClients() const;

    /** Quita de la lista los ClientWorker cuyo hilo ya terminó (p. ej. tras desconexión). Seguro desde el hilo de mensajes / UI. */
    void purgeFinishedClients();

    std::vector<StreamingClient> getClients() const;

    void run() override;

    void registerStreamingQueue (const std::shared_ptr<PerClientAudioQueue>& q);

    void unregisterStreamingQueue (PerClientAudioQueue* q);

    std::vector<std::shared_ptr<PerClientAudioQueue>> snapshotStreamingQueues() const;

    bool isAudioBroadcastServiceRunning() const noexcept;

private:
    AudioBufferManager* audioBufferManager = nullptr;
    SynchronizationEngine* synchronizationEngine = nullptr;
    int serverPort = 8080;
    std::atomic<bool> serverActive{false};
    std::atomic<bool> shouldStop{false};
    std::atomic<bool> broadcastServiceRunning{false};

    static constexpr bool ENABLE_DEBUG_LOGS =
    #if JUCE_DEBUG
        true
    #else
        false
    #endif
    ;

    mutable juce::CriticalSection clientsLock;
    std::vector<std::unique_ptr<ClientWorker>> clients;

    mutable juce::CriticalSection streamingQueuesLock;
    std::vector<std::shared_ptr<PerClientAudioQueue>> streamingQueues;

    juce::StreamingSocket* serverSocket = nullptr;

    std::unique_ptr<juce::Thread> audioBroadcastThread;

    void cleanupFinishedWorkers();

    void debugLog (const juce::String& message) const;

    void signalAllStreamQueuesShutdown();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NetworkStreamer)
};
