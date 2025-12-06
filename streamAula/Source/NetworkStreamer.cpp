/*
  ==============================================================================

    NetworkStreamer.cpp
    Implementación del servidor HTTP para streaming de audio.

  ==============================================================================
*/

#include "NetworkStreamer.h"

//==============================================================================
NetworkStreamer::NetworkStreamer(AudioBufferManager* bufferManager, int port)
    : juce::Thread("NetworkStreamer"),
      audioBufferManager(bufferManager),
      serverPort(port),
      serverSocket(nullptr)
{
    jassert(bufferManager != nullptr);
}

NetworkStreamer::~NetworkStreamer()
{
    stopServer();
}

//==============================================================================
bool NetworkStreamer::startServer()
{
    if (serverActive.load())
        return true; // Ya está activo
    
    // Crear socket del servidor usando juce::StreamingSocket
    // Nota: Esta implementación es básica y puede necesitar ajustes según la versión de JUCE
    serverSocket = new juce::StreamingSocket();
    
    if (serverSocket->createListener(serverPort, juce::String()))
    {
        serverActive.store(true);
        shouldStop.store(false);
        
        // Iniciar thread
        startThread();
        
        return true;
    }
    else
    {
        delete serverSocket;
        serverSocket = nullptr;
        return false;
    }
}

//==============================================================================
void NetworkStreamer::stopServer()
{
    if (!serverActive.load())
        return;
    
    shouldStop.store(true);
    
    // Cerrar socket para despertar el thread
    if (serverSocket != nullptr)
    {
        serverSocket->close();
        delete serverSocket;
        serverSocket = nullptr;
    }
    
    // Esperar a que el thread termine
    stopThread(2000);
    
    // Limpiar
    serverActive.store(false);
    
    // Limpiar clientes
    {
        const juce::ScopedLock lock(clientsLock);
        clients.clear();
    }
}

//==============================================================================
int NetworkStreamer::getNumClients() const
{
    const juce::ScopedLock lock(clientsLock);
    return (int)clients.size();
}

//==============================================================================
std::vector<StreamingClient> NetworkStreamer::getClients() const
{
    const juce::ScopedLock lock(clientsLock);
    
    std::vector<StreamingClient> result;
    for (const auto& client : clients)
    {
        if (client->isActive)
            result.push_back(*client);
    }
    return result;
}

//==============================================================================
void NetworkStreamer::run()
{
    // Thread principal del servidor
    // NOTA: Esta es una implementación simplificada
    // Para una implementación completa, necesitarías usar la API correcta de JUCE
    // que puede variar según la versión. Por ahora, el servidor se inicia
    // pero la aceptación de conexiones necesita ser implementada según la API disponible.
    
    while (!threadShouldExit() && !shouldStop.load())
    {
        if (serverSocket == nullptr)
            break;
        
        // TODO: Implementar aceptación de conexiones según API de JUCE disponible
        // Por ahora, el servidor está activo pero no acepta conexiones
        // Esto se puede implementar usando:
        // - juce::StreamingSocket::waitForNextConnection() (si está disponible)
        // - O una implementación alternativa con sockets nativos
        
        juce::Thread::sleep(100);
    }
}

//==============================================================================
void NetworkStreamer::handleClientConnection(juce::StreamingSocket* clientSocket)
{
    if (clientSocket == nullptr)
        return;
    
    // Leer solicitud HTTP
    juce::MemoryBlock buffer(4096);
    int bytesRead = clientSocket->read(buffer.getData(), (int)buffer.getSize(), false);
    
    if (bytesRead > 0)
    {
        juce::String request((const char*)buffer.getData(), (size_t)bytesRead);
        processHttpRequest(clientSocket, request);
    }
}

//==============================================================================
void NetworkStreamer::processHttpRequest(juce::StreamingSocket* socket, const juce::String& request)
{
    if (socket == nullptr)
        return;
    
    // Parsear solicitud HTTP básica
    if (request.startsWith("GET /stream"))
    {
        // Endpoint de streaming
        juce::String response = "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: application/octet-stream\r\n";
        response << "Connection: keep-alive\r\n";
        response << "Transfer-Encoding: chunked\r\n";
        response << "\r\n";
        
        socket->write(response.toRawUTF8(), response.length());
        
        // Enviar audio continuamente
        juce::AudioBuffer<float> audioChunk;
        const int chunkSize = 1024; // ~23ms a 44.1kHz
        
        while (!shouldStop.load() && socket->isConnected())
        {
            if (audioBufferManager != nullptr)
            {
                // Leer del buffer circular (lock-free)
                if (audioBufferManager->readFromBuffer(audioChunk, chunkSize))
                {
                    sendAudioToClient(socket, audioChunk);
                }
                else
                {
                    // No hay suficientes datos, esperar un poco
                    juce::Thread::sleep(5);
                }
            }
            else
            {
                break;
            }
        }
    }
    else if (request.startsWith("GET /config"))
    {
        // Endpoint de configuración
        juce::String json = "{";
        json << "\"sampleRate\":" << (audioBufferManager != nullptr ? audioBufferManager->getSampleRate() : 44100) << ",";
        json << "\"channels\":" << (audioBufferManager != nullptr ? audioBufferManager->getNumChannels() : 2) << ",";
        json << "\"port\":" << serverPort;
        json << "}";
        
        juce::String response = "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Content-Length: " << json.length() << "\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "\r\n";
        response << json;
        
        socket->write(response.toRawUTF8(), response.length());
    }
    else
    {
        // 404 Not Found
        juce::String response = "HTTP/1.1 404 Not Found\r\n\r\n";
        socket->write(response.toRawUTF8(), response.length());
    }
}

//==============================================================================
void NetworkStreamer::sendAudioToClient(juce::StreamingSocket* socket, const juce::AudioBuffer<float>& audio)
{
    if (socket == nullptr || !socket->isConnected())
        return;
    
    // Convertir audio a bytes (PCM float32)
    const int numSamples = audio.getNumSamples();
    const int numChannels = audio.getNumChannels();
    const int dataSize = numSamples * numChannels * sizeof(float);
    
    // Enviar como chunk HTTP
    juce::String chunkHeader = juce::String::toHexString(dataSize) + "\r\n";
    socket->write(chunkHeader.toRawUTF8(), chunkHeader.length());
    
    // Enviar datos de audio (interleaved)
    juce::HeapBlock<float> interleaved(numSamples * numChannels);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            interleaved[sample * numChannels + channel] = audio.getSample(channel, sample);
        }
    }
    
    socket->write(interleaved.getData(), dataSize);
    
    // Chunk terminator
    juce::String chunkEnd = "\r\n";
    socket->write(chunkEnd.toRawUTF8(), chunkEnd.length());
}

//==============================================================================
void NetworkStreamer::addClient(const juce::String& clientId)
{
    const juce::ScopedLock lock(clientsLock);
    
    auto client = std::make_unique<StreamingClient>();
    client->clientId = clientId;
    client->connectionTime = juce::Time::getCurrentTime();
    client->isActive = true;
    
    clients.push_back(std::move(client));
}

//==============================================================================
void NetworkStreamer::removeClient(const juce::String& clientId)
{
    const juce::ScopedLock lock(clientsLock);
    
    clients.erase(
        std::remove_if(clients.begin(), clients.end(),
            [&clientId](const std::unique_ptr<StreamingClient>& client) {
                return client->clientId == clientId;
            }),
        clients.end()
    );
}

//==============================================================================
void NetworkStreamer::cleanupInactiveClients()
{
    const juce::ScopedLock lock(clientsLock);
    
    clients.erase(
        std::remove_if(clients.begin(), clients.end(),
            [](const std::unique_ptr<StreamingClient>& client) {
                return !client->isActive;
            }),
        clients.end()
    );
}

