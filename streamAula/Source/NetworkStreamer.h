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
#include <memory>
#include <vector>
#include <atomic>

//==============================================================================
/**
    Cliente conectado al servidor de streaming.
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
        @param port             Puerto donde escuchar (por defecto 8080)
    */
    NetworkStreamer(AudioBufferManager* bufferManager, int port = 8080);
    
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
    int serverPort;
    std::atomic<bool> serverActive{false};
    std::atomic<bool> shouldStop{false};
    
    // Lista de clientes conectados (protegida por mutex)
    mutable juce::CriticalSection clientsLock;
    std::vector<std::unique_ptr<StreamingClient>> clients;
    
    // Socket del servidor
    // Nota: Implementación básica - puede necesitar ajustes según versión de JUCE
    juce::StreamingSocket* serverSocket;
    
    //==============================================================================
    /**
        Manejar conexión de cliente.
    */
    void handleClientConnection(juce::StreamingSocket* clientSocket);
    
    /**
        Enviar audio a un cliente.
    */
    void sendAudioToClient(juce::StreamingSocket* socket, const juce::AudioBuffer<float>& audio);
    
    /**
        Procesar solicitud HTTP básica.
    */
    void processHttpRequest(juce::StreamingSocket* socket, const juce::String& request);
    
    
    /**
        Agregar cliente.
    */
    void addClient(const juce::String& clientId);
    
    /**
        Remover cliente.
    */
    void removeClient(const juce::String& clientId);
    
    /**
        Limpiar clientes inactivos.
    */
    void cleanupInactiveClients();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NetworkStreamer)
};

