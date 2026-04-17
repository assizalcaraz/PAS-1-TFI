/*
  ==============================================================================

    NetworkStreamer.h
    Servidor HTTP embebido para streaming de audio a múltiples clientes.
    
    Implementa un servidor HTTP que lee del AudioBufferManager y envía
    audio a múltiples clientes conectados simultáneamente.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "AudioBufferManager.h"
#include "SynchronizationEngine.h"
#include <memory>
#include <vector>
#include <atomic>

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
    - Streaming continuo de audio para /stream
*/
class ClientWorker : public juce::Thread
{
public:
    /**
        Constructor.
        
        @param clientSocket        Socket del cliente (ClientWorker toma propiedad)
        @param bufferManager       Puntero al AudioBufferManager (no es dueño)
        @param syncEngine          Puntero al SynchronizationEngine (no es dueño, opcional)
    */
    ClientWorker(juce::StreamingSocket* clientSocket,
                 AudioBufferManager* bufferManager,
                 SynchronizationEngine* syncEngine = nullptr);
    
    /**
        Destructor.
        Asegura que el thread se detenga correctamente.
    */
    ~ClientWorker() override;
    
    /**
        Thread principal del worker (override de juce::Thread).
        Maneja la conexión completa del cliente.
    */
    void run() override;
    
    /**
        Obtener información del cliente como StreamingClient.
    */
    StreamingClient getClientInfo() const;
    
    /**
        Verificar si el cliente está activo.
    */
    bool isClientActive() const noexcept { return isActive; }
    
    /**
        Detener el streaming de forma segura.
    */
    void stopStreaming();
    
private:
    //==============================================================================
    std::unique_ptr<juce::StreamingSocket> clientSocket;
    AudioBufferManager* audioBufferManager;
    SynchronizationEngine* synchronizationEngine;
    
    std::atomic<bool> shouldStop{false};
    
    // Buffer temporal de audio (evita allocaciones en cada iteración)
    juce::AudioBuffer<float> audioChunk;
    
    // Información del cliente
    juce::String clientId;
    juce::Time connectionTime;
    int64_t lastSampleSent{0};
    bool isActive{true};
    
    // Flag para logs de debug
    static constexpr bool ENABLE_DEBUG_LOGS = 
    #if JUCE_DEBUG
        true
    #else
        false
    #endif
    ;
    
    //==============================================================================
    /**
        Procesar solicitud HTTP.
    */
    void processHttpRequest(const juce::String& request);
    
    /**
        Enviar audio al cliente.
    */
    void sendAudioToClient(const juce::AudioBuffer<float>& audio);
    
    /**
        Helper para logging condicional (solo en debug).
    */
    void debugLog(const juce::String& message) const;
    
    /**
        Validar que los datos leídos son texto HTTP válido.
    */
    static bool isValidHttpText(const void* data, int size);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClientWorker)
};

//==============================================================================
/**
    Servidor HTTP para streaming de audio.
    
    Lee audio del AudioBufferManager (lock-free) y lo envía a múltiples
    clientes conectados simultáneamente.
    
    Características:
    - Thread separado para networking (no bloquea audio thread)
    - Múltiples clientes concurrentes
    - Streaming continuo de audio
    - Endpoint /stream para audio
    - Endpoint /config para configuración
*/
class NetworkStreamer : public juce::Thread
{
public:
    //==============================================================================
    /**
        Constructor.
        
        @param bufferManager    Puntero al AudioBufferManager (no es dueño)
        @param syncEngine       Puntero al SynchronizationEngine (no es dueño, opcional)
        @param port             Puerto donde escuchar (por defecto 8080)
    */
    NetworkStreamer(AudioBufferManager* bufferManager, 
                    SynchronizationEngine* syncEngine = nullptr,
                    int port = 8080);
    
    /**
        Destructor.
    */
    ~NetworkStreamer() override;
    
    //==============================================================================
    /**
        Iniciar el servidor.
        
        @return true si se inició correctamente
    */
    bool startServer();
    
    /**
        Detener el servidor.
    */
    void stopServer();
    
    /**
        Verificar si el servidor está activo.
    */
    bool isServerActive() const noexcept { return serverActive.load(); }
    
    /**
        Obtener el puerto en el que está escuchando.
    */
    int getPort() const noexcept { return serverPort; }
    
    /**
        Obtener el número de clientes conectados.
    */
    int getNumClients() const;
    
    /**
        Obtener información de los clientes conectados.
    */
    std::vector<StreamingClient> getClients() const;
    
    //==============================================================================
    /**
        Thread principal del servidor (override de juce::Thread).
    */
    void run() override;

private:
    //==============================================================================
    AudioBufferManager* audioBufferManager;
    SynchronizationEngine* synchronizationEngine;
    int serverPort;
    std::atomic<bool> serverActive{false};
    std::atomic<bool> shouldStop{false};
    
    // Flag para logs de debug (reducir ruido en Release)
    static constexpr bool ENABLE_DEBUG_LOGS = 
    #if JUCE_DEBUG
        true
    #else
        false
    #endif
    ;
    
    // Lista de clientes conectados (protegida por mutex)
    mutable juce::CriticalSection clientsLock;
    std::vector<std::unique_ptr<ClientWorker>> clients;
    
    // Socket del servidor
    // Nota: Implementación básica - puede necesitar ajustes según versión de JUCE
    juce::StreamingSocket* serverSocket;
    
    //==============================================================================
    /**
        Limpiar threads de clientes terminados.
    */
    void cleanupFinishedWorkers();
    
    /**
        Helper para logging condicional (solo en debug).
    */
    void debugLog(const juce::String& message) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NetworkStreamer)
};

