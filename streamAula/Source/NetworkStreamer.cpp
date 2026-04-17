/*
  ==============================================================================

    NetworkStreamer.cpp
    Implementación del servidor HTTP para streaming de audio.

  ==============================================================================
*/

#include "NetworkStreamer.h"
#include "JuceHeader.h" // Para juce::Logger
#include <signal.h> // Para manejar SIGPIPE
#include <cstring> // Para std::memcpy
#include <cmath> // Para std::sqrt (RMS)
#include <cstdint> // Para int16_t

// BinaryData.h se genera automáticamente por Projucer cuando se agregan archivos a Binary Resources
// Incluir siempre - Projucer lo genera automáticamente
#include "BinaryData.h"

// Helper para crear String desde literales UTF-8 de forma segura
// Evita problemas de aserción en juce_String.cpp con caracteres no ASCII
static inline juce::String U8(const char* utf8) 
{ 
    return juce::String(juce::CharPointer_UTF8(utf8)); 
}

// Helper para construir strings UTF-8 desde bytes individuales (evita problemas con \x que consume múltiples dígitos)
static inline juce::String U8_BYTES(const uint8_t* bytes, size_t count)
{
    juce::MemoryBlock mb(bytes, count);
    return juce::String(juce::CharPointer_UTF8(static_cast<const char*>(mb.getData())));
}

//==============================================================================
// ClientWorker Implementation
//==============================================================================

ClientWorker::ClientWorker(juce::StreamingSocket* clientSocket,
                           AudioBufferManager* bufferManager,
                           SynchronizationEngine* syncEngine)
    : juce::Thread("ClientWorker"),
      clientSocket(clientSocket),
      audioBufferManager(bufferManager),
      synchronizationEngine(syncEngine),
      connectionTime(juce::Time::getCurrentTime())
{
    jassert(clientSocket != nullptr);
    jassert(bufferManager != nullptr);
    
    // Generar ID único para este cliente
    clientId = juce::String::formatted("client_%lld", connectionTime.toMilliseconds());
}

ClientWorker::~ClientWorker()
{
    stopStreaming();
    stopThread(2000);
    
    if (clientSocket != nullptr)
    {
        clientSocket->close();
    }
}

void ClientWorker::stopStreaming()
{
    shouldStop.store(true);
}

StreamingClient ClientWorker::getClientInfo() const
{
    StreamingClient info;
    info.clientId = clientId;
    info.connectionTime = connectionTime;
    info.lastSampleSent = lastSampleSent;
    info.isActive = isActive;
    return info;
}

void ClientWorker::run()
{
    if (clientSocket == nullptr || !clientSocket->isConnected())
        return;
    
    debugLog("ClientWorker: Thread started for client: " + clientId);
    
    // Leer solicitud HTTP
    juce::MemoryBlock buffer(4096);
    int bytesRead = 0;
    int attempts = 0;
    const int maxAttempts = 10;
    
    while (attempts < maxAttempts && bytesRead == 0 && !threadShouldExit() && !shouldStop.load())
    {
        bytesRead = clientSocket->read(buffer.getData(), (int)buffer.getSize(), false);
        
        if (bytesRead > 0)
            break;
        
        if (bytesRead < 0)
            return;
        
        if (attempts < maxAttempts - 1)
            juce::Thread::sleep(50);
        
        attempts++;
        
        if (!clientSocket->isConnected())
            return;
    }
    
    if (bytesRead > 0)
    {
        // Validar que los datos son texto HTTP válido
        if (!isValidHttpText(buffer.getData(), bytesRead))
        {
            debugLog(U8("ClientWorker: Datos recibidos no son texto HTTP v\xc3\xa1lido"));
            return;
        }
        
        // Buscar el final del header HTTP
        const char* data = static_cast<const char*>(buffer.getData());
        const char* headerEnd = nullptr;
        
        for (int i = 0; i < bytesRead - 3; ++i)
        {
            if (data[i] == '\r' && data[i+1] == '\n' && 
                data[i+2] == '\r' && data[i+3] == '\n')
            {
                headerEnd = data + i + 4;
                break;
            }
        }
        
        int textLength = (headerEnd != nullptr) ? 
            static_cast<int>(headerEnd - data) : bytesRead;
        
        if (textLength > bytesRead)
            textLength = bytesRead;
        
        if (textLength <= 0)
        {
            debugLog("ClientWorker: textLength es 0 o negativo, rechazando request");
            return;
        }
        
        // Validar caracteres
        const char* safeData = static_cast<const char*>(buffer.getData());
        bool isValidText = true;
        for (int i = 0; i < textLength; ++i)
        {
            unsigned char c = static_cast<unsigned char>(safeData[i]);
            if (!(c >= 32 && c <= 126) && c != '\r' && c != '\n' && c != '\t' && c != 0)
            {
                if (i < 50)
                {
                    isValidText = false;
                    break;
                }
            }
        }
        
        if (!isValidText)
        {
            debugLog(U8("ClientWorker: Datos contienen caracteres no v\xc3\xa1lidos en header HTTP"));
            return;
        }
        
        // Construir String de forma segura
        const char* start = safeData;
        const char* end = start + textLength;
        juce::CharPointer_UTF8 utf8Start(start);
        juce::CharPointer_UTF8 utf8End(end);
        juce::String request(utf8Start, utf8End);
        
        if (request.isEmpty() && textLength > 0)
        {
            debugLog(U8("ClientWorker: Request est\xc3\xa1 vac\xc3\xado despu\xc3\xa9s de parseo"));
            return;
        }
        
        if (ENABLE_DEBUG_LOGS)
        {
            juce::String safeRequest = request;
            for (int i = safeRequest.length() - 1; i >= 0; --i)
            {
                juce::juce_wchar c = safeRequest[i];
                if (c < 32 && c != '\r' && c != '\n' && c != '\t')
                {
                    safeRequest = safeRequest.substring(0, i) + safeRequest.substring(i + 1);
                }
            }
            juce::String truncatedRequest = safeRequest.substring(0, 100);
            juce::String logMsg("ClientWorker: Request recibido (");
            logMsg += juce::String(bytesRead);
            logMsg += " bytes): ";
            logMsg += truncatedRequest;
            debugLog(logMsg);
        }
        
        processHttpRequest(request);
    }
    
    debugLog("ClientWorker: Thread finished for client: " + clientId);
    isActive = false;
}

void ClientWorker::processHttpRequest(const juce::String& request)
{
    if (clientSocket == nullptr || !clientSocket->isConnected())
        return;
    
    if (request.startsWith("GET /stream"))
    {
        // Endpoint de streaming
        debugLog("ClientWorker: Procesando request /stream, iniciando streaming...");
        
        juce::String response = "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: application/octet-stream\r\n";
        response << "Connection: keep-alive\r\n";
        response << "Transfer-Encoding: chunked\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "\r\n";
        
        if (!clientSocket->isConnected())
        {
            debugLog("ClientWorker: Socket no conectado al iniciar /stream");
            return;
        }
        
        int bytesWritten = clientSocket->write(response.toRawUTF8(), (int)response.length());
        if (bytesWritten < 0)
        {
            debugLog("ClientWorker: Error al escribir respuesta HTTP en /stream");
            return;
        }
        
        debugLog("ClientWorker: Respuesta HTTP enviada, iniciando loop de streaming...");
        
        // Descartar datos antiguos del buffer para nuevos clientes
        if (audioBufferManager != nullptr)
        {
            int availableSamples = audioBufferManager->getAvailableSamples();
            const int targetBufferSize = 1024;
            
            if (availableSamples > targetBufferSize)
            {
                int samplesToDiscard = availableSamples - targetBufferSize;
                juce::AudioBuffer<float> discardBuffer;
                discardBuffer.setSize(audioBufferManager->getNumChannels(), samplesToDiscard, false, false, true);
                
                while (audioBufferManager->getAvailableSamples() > targetBufferSize)
                {
                    int currentAvailable = audioBufferManager->getAvailableSamples();
                    int toDiscard = juce::jmin(currentAvailable - targetBufferSize, discardBuffer.getNumSamples());
                    if (toDiscard > 0)
                    {
                        discardBuffer.setSize(audioBufferManager->getNumChannels(), toDiscard, false, false, true);
                        audioBufferManager->readFromBuffer(discardBuffer, toDiscard);
                    }
                    else
                    {
                        break;
                    }
                }
                
                #if JUCE_DEBUG
                juce::String skipMsg("ClientWorker: Cliente nuevo - descartados ");
                skipMsg += juce::String(samplesToDiscard);
                skipMsg += U8(" samples antiguos");
                debugLog(skipMsg);
                #endif
            }
        }
        
        // Bucle de streaming
        const int chunkSize = 1024;
        int chunksSent = 0;
        
        int numChannels = 2;
        int sampleRate = 44100;
        if (audioBufferManager != nullptr)
        {
            numChannels = audioBufferManager->getNumChannels();
            sampleRate = audioBufferManager->getSampleRate();
            
            juce::String configMsg = U8("ClientWorker: Configuraci\xc3\xb3n - canales: ");
            configMsg += juce::String(numChannels);
            configMsg += ", sampleRate: ";
            configMsg += juce::String(sampleRate);
            debugLog(configMsg);
        }
        
        while (!threadShouldExit() && !shouldStop.load() && clientSocket->isConnected())
        {
            if (audioBufferManager != nullptr)
            {
                int availableSamples = audioBufferManager->getAvailableSamples();
                
                bool gotChunk = false;
                
                if (availableSamples >= chunkSize)
                {
                    if (synchronizationEngine != nullptr)
                    {
                        gotChunk = synchronizationEngine->getSynchronizedChunk(audioChunk, chunkSize, 0);
                    }
                    else
                    {
                        gotChunk = audioBufferManager->readFromBuffer(audioChunk, chunkSize);
                    }
                }
                
                if (gotChunk && audioChunk.getNumSamples() > 0)
                {
                    sendAudioToClient(audioChunk);
                    chunksSent++;
                    lastSampleSent += audioChunk.getNumSamples();
                    
                    // Control de velocidad adaptativo
                    int freeSpace = audioBufferManager->getFreeSpace();
                    int bufferSize = audioBufferManager->getBufferSize();
                    double latencyMs = (double)availableSamples / sampleRate * 1000.0;
                    double chunkDurationSeconds = (double)chunkSize / sampleRate;
                    int sleepMs = (int)(chunkDurationSeconds * 1000.0);
                    
                    if (latencyMs > 500.0) {
                        sleepMs = (int)(sleepMs * 0.05);
                    } else if (latencyMs > 300.0) {
                        sleepMs = (int)(sleepMs * 0.2);
                    } else if (latencyMs > 200.0) {
                        sleepMs = (int)(sleepMs * 0.5);
                    } else if (latencyMs < 100.0) {
                        sleepMs = (int)(sleepMs * 1.2);
                    }
                    
                    if (sleepMs > 0 && sleepMs < 25) {
                        juce::Thread::sleep(sleepMs);
                    }
                }
                else
                {
                    // No hay datos disponibles, enviar silencio
                    audioChunk.setSize(numChannels, chunkSize, false, false, true);
                    audioChunk.clear();
                    sendAudioToClient(audioChunk);
                    chunksSent++;
                    juce::Thread::sleep(10);
                }
            }
            else
            {
                // AudioBufferManager no disponible, enviar silencio
                audioChunk.setSize(numChannels, chunkSize, false, false, true);
                audioChunk.clear();
                sendAudioToClient(audioChunk);
                chunksSent++;
                juce::Thread::sleep(10);
            }
        }
        
        // Enviar chunk final
        juce::String finalChunk = "0\r\n\r\n";
        if (clientSocket->isConnected())
        {
            clientSocket->write(finalChunk.toRawUTF8(), (int)finalChunk.length());
        }
        
        juce::String endMsg("ClientWorker: Streaming finalizado - total chunks enviados: ");
        endMsg += juce::String(chunksSent);
        debugLog(endMsg);
    }
    else if (request.startsWith("GET /config"))
    {
        // Endpoint de configuración
        juce::String localIP = "localhost";
        juce::Array<juce::IPAddress> addresses;
        juce::IPAddress::findAllAddresses(addresses);
        for (const auto& addr : addresses)
        {
            juce::String ipStr = addr.toString();
            bool isLoopback = (ipStr == "127.0.0.1" || ipStr == "::1" || ipStr.startsWith("127."));
            
            if (!isLoopback && ipStr.contains("."))
            {
                if (ipStr.startsWith("192.168.") || ipStr.startsWith("10.") || 
                    ipStr.startsWith("172.16.") || ipStr.startsWith("172.17.") ||
                    ipStr.startsWith("172.18.") || ipStr.startsWith("172.19.") ||
                    ipStr.startsWith("172.20.") || ipStr.startsWith("172.21.") ||
                    ipStr.startsWith("172.22.") || ipStr.startsWith("172.23.") ||
                    ipStr.startsWith("172.24.") || ipStr.startsWith("172.25.") ||
                    ipStr.startsWith("172.26.") || ipStr.startsWith("172.27.") ||
                    ipStr.startsWith("172.28.") || ipStr.startsWith("172.29.") ||
                    ipStr.startsWith("172.30.") || ipStr.startsWith("172.31."))
                {
                    localIP = ipStr;
                    break;
                }
            }
        }
        
        juce::String json = "{";
        json << "\"sampleRate\":" << (audioBufferManager != nullptr ? audioBufferManager->getSampleRate() : 44100) << ",";
        json << "\"channels\":" << (audioBufferManager != nullptr ? audioBufferManager->getNumChannels() : 2) << ",";
        json << "\"port\":" << (audioBufferManager != nullptr ? 8080 : 8080) << ",";
        json << "\"ip\":\"" << localIP << "\"";
        json << "}";
        
        juce::String response = "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Content-Length: " << json.length() << "\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "\r\n";
        response << json;
        
        if (clientSocket->isConnected())
        {
            int bytesWritten = clientSocket->write(response.toRawUTF8(), (int)response.length());
            if (bytesWritten < 0)
                return;
        }
    }
    else if (request.startsWith("GET /sync"))
    {
        // Endpoint de sincronización
        int64_t timestamp = 0;
        int64_t sequenceNumber = 0;
        bool hasSyncInfo = false;
        
        if (synchronizationEngine != nullptr)
        {
            hasSyncInfo = synchronizationEngine->getSyncInfo(timestamp, sequenceNumber);
        }
        
        juce::String json = "{";
        json << "\"timestamp\":" << timestamp << ",";
        json << "\"sequenceNumber\":" << sequenceNumber << ",";
        json << "\"hasSyncInfo\":" << (hasSyncInfo ? "true" : "false") << ",";
        json << "\"currentSampleCount\":" << (audioBufferManager != nullptr ? audioBufferManager->getCurrentSampleCount() : 0) << ",";
        json << "\"syncBufferSize\":" << (synchronizationEngine != nullptr ? synchronizationEngine->getSyncBufferSize() : 0) << ",";
        json << "\"currentBufferSize\":" << (synchronizationEngine != nullptr ? synchronizationEngine->getCurrentBufferSize() : 0) << ",";
        json << "\"averageLatency\":" << (synchronizationEngine != nullptr ? synchronizationEngine->getAverageLatency() : 0.0);
        json << "}";
        
        juce::String response = "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Content-Length: " << json.length() << "\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "\r\n";
        response << json;
        
        if (clientSocket->isConnected())
        {
            int bytesWritten = clientSocket->write(response.toRawUTF8(), (int)response.length());
            if (bytesWritten < 0)
                return;
        }
    }
    else if (request.startsWith("GET /favicon.ico"))
    {
        juce::String response = "HTTP/1.1 204 No Content\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        
        if (clientSocket->isConnected())
        {
            int bytesWritten = clientSocket->write(response.toRawUTF8(), (int)response.length());
            if (bytesWritten < 0)
                return;
        }
    }
    else if (request.startsWith("GET /app.js"))
    {
        const void* data = BinaryData::app_js;
        const int size = BinaryData::app_jsSize;
        
        if (data != nullptr && size > 0)
        {
            juce::String response;
            response << "HTTP/1.1 200 OK\r\n";
            response << "Content-Type: application/javascript\r\n";
            response << "Content-Length: " << size << "\r\n";
            response << "Access-Control-Allow-Origin: *\r\n";
            response << "Connection: close\r\n\r\n";
            
            if (clientSocket->isConnected())
            {
                clientSocket->write(response.toRawUTF8(), (int)response.length());
                clientSocket->write(data, size);
            }
            return;
        }
        else
        {
            juce::String response = "HTTP/1.1 404 Not Found\r\n";
            response << "Content-Type: text/plain\r\n";
            response << "Content-Length: 45\r\n";
            response << "\r\n";
            response << U8("404 Not Found - app.js no agregado a BinaryData");
            
            if (clientSocket->isConnected())
            {
                clientSocket->write(response.toRawUTF8(), (int)response.length());
            }
            return;
        }
    }
    else if (request.startsWith("GET /styles.css"))
    {
        const void* data = BinaryData::styles_css;
        const int size = BinaryData::styles_cssSize;
        
        if (data != nullptr && size > 0)
        {
            juce::String response;
            response << "HTTP/1.1 200 OK\r\n";
            response << "Content-Type: text/css\r\n";
            response << "Content-Length: " << size << "\r\n";
            response << "Access-Control-Allow-Origin: *\r\n";
            response << "Connection: close\r\n\r\n";
            
            if (clientSocket->isConnected())
            {
                clientSocket->write(response.toRawUTF8(), (int)response.length());
                clientSocket->write(data, size);
            }
            return;
        }
        else
        {
            juce::String response = "HTTP/1.1 404 Not Found\r\n";
            response << "Content-Type: text/plain\r\n";
            response << "Content-Length: 48\r\n";
            response << "\r\n";
            response << U8("404 Not Found - styles.css no agregado a BinaryData");
            
            if (clientSocket->isConnected())
            {
                clientSocket->write(response.toRawUTF8(), (int)response.length());
            }
            return;
        }
    }
    else if (request.startsWith("GET / ") || request.startsWith("GET /HTTP") || 
             request.startsWith("GET /index.html") || 
             (request.startsWith("GET /") && !request.startsWith("GET /stream") && 
              !request.startsWith("GET /config") && !request.startsWith("GET /sync") &&
              !request.startsWith("GET /favicon.ico") && !request.startsWith("GET /app.js") &&
              !request.startsWith("GET /styles.css")))
    {
        const void* data = BinaryData::index_html;
        const int size = BinaryData::index_htmlSize;
        
        if (data != nullptr && size > 0)
        {
            juce::String response;
            response << "HTTP/1.1 200 OK\r\n";
            response << "Content-Type: text/html; charset=utf-8\r\n";
            response << "Content-Length: " << size << "\r\n";
            response << "Access-Control-Allow-Origin: *\r\n";
            response << "Connection: close\r\n\r\n";
            
            if (clientSocket->isConnected())
            {
                clientSocket->write(response.toRawUTF8(), (int)response.length());
                clientSocket->write(data, size);
            }
            return;
        }
        else
        {
            juce::String response = "HTTP/1.1 404 Not Found\r\n";
            response << "Content-Type: text/plain\r\n";
            response << "Content-Length: 50\r\n";
            response << "\r\n";
            response << U8("404 Not Found - index.html no agregado a BinaryData");
            
            if (clientSocket->isConnected())
            {
                clientSocket->write(response.toRawUTF8(), (int)response.length());
            }
            return;
        }
    }
    else
    {
        juce::String response = "HTTP/1.1 404 Not Found\r\n";
        response << "Content-Type: text/plain\r\n";
        response << "Content-Length: 13\r\n";
        response << "\r\n";
        response << "404 Not Found";
        
        if (clientSocket->isConnected())
        {
            int bytesWritten = clientSocket->write(response.toRawUTF8(), (int)response.length());
            if (bytesWritten < 0)
                return;
        }
    }
}

void ClientWorker::sendAudioToClient(const juce::AudioBuffer<float>& audio)
{
    if (clientSocket == nullptr || !clientSocket->isConnected())
        return;
    
    const int numSamples = audio.getNumSamples();
    const int numChannels = audio.getNumChannels();
    const int dataSize = numSamples * numChannels * sizeof(int16_t);
    
    juce::String chunkHeader = juce::String::toHexString(dataSize).toLowerCase() + "\r\n";
    
    if (!clientSocket->isConnected())
        return;
    
    int bytesWritten = clientSocket->write(chunkHeader.toRawUTF8(), (int)chunkHeader.length());
    if (bytesWritten < 0)
        return;
    
    // Convertir float32 a int16 (interleaved)
    juce::HeapBlock<int16_t> interleavedInt16(numSamples * numChannels);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float sampleValue = audio.getSample(channel, sample);
            sampleValue = juce::jlimit(-1.0f, 1.0f, sampleValue);
            int16_t int16Value = (int16_t)(sampleValue * 32767.0f);
            interleavedInt16[sample * numChannels + channel] = int16Value;
        }
    }
    
    if (!clientSocket->isConnected())
        return;
    
    bytesWritten = clientSocket->write(interleavedInt16.getData(), dataSize);
    if (bytesWritten < 0)
        return;
    
    juce::String chunkEnd = "\r\n";
    
    if (!clientSocket->isConnected())
        return;
    
    clientSocket->write(chunkEnd.toRawUTF8(), (int)chunkEnd.length());
}

void ClientWorker::debugLog(const juce::String& message) const
{
    if (ENABLE_DEBUG_LOGS)
    {
        juce::Logger::writeToLog(message);
    }
}

bool ClientWorker::isValidHttpText(const void* data, int size)
{
    if (data == nullptr || size <= 0)
        return false;
    
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    
    for (int i = 0; i < size && i < 1024; ++i)
    {
        unsigned char c = bytes[i];
        
        if (c >= 32 && c <= 126)
            continue;
        else if (c == 13 || c == 10 || c == 9)
            continue;
        else if (c == 0)
        {
            if (i < 10)
                return false;
            continue;
        }
        else
        {
            if (i < 20)
                return false;
        }
    }
    
    return true;
}

//==============================================================================
// NetworkStreamer Implementation
//==============================================================================

NetworkStreamer::NetworkStreamer(AudioBufferManager* bufferManager, 
                                 SynchronizationEngine* syncEngine,
                                 int port)
    : juce::Thread("NetworkStreamer"),
      audioBufferManager(bufferManager),
      synchronizationEngine(syncEngine),
      serverPort(port),
      serverSocket(nullptr)
{
    jassert(bufferManager != nullptr);
    
    // Ignorar SIGPIPE para evitar que el proceso termine cuando se escribe a un socket cerrado
    // Esto es común en servidores cuando el cliente cierra la conexión abruptamente
    #if JUCE_MAC || JUCE_LINUX
    signal(SIGPIPE, SIG_IGN);
    #endif
}

NetworkStreamer::~NetworkStreamer()
{
    stopServer();
}

//==============================================================================
bool NetworkStreamer::startServer()
{
    // Verificar que no esté ya activo (evitar reinicios en bucle)
    if (serverActive.load())
    {
        debugLog(U8("NetworkStreamer: Servidor ya est\xc3\xa1 activo, ignorando startServer()"));
        return true; // Ya está activo
    }
    
    // Verificar que el thread no esté corriendo
    if (isThreadRunning())
    {
        debugLog(U8("NetworkStreamer: Thread ya est\xc3\xa1 corriendo, deteniendo antes de reiniciar..."));
        stopServer(); // Detener primero
        juce::Thread::sleep(100); // Dar tiempo para que se detenga
    }
    
    // Crear socket del servidor usando juce::StreamingSocket
    // Nota: Esta implementación es básica y puede necesitar ajustes según la versión de JUCE
    serverSocket = new juce::StreamingSocket();
    
    // Intentar crear listener en todas las interfaces (0.0.0.0)
    // Esto permite conexiones desde otras máquinas en la red local
    // Usar "0.0.0.0" explícitamente para asegurar que escucha en todas las interfaces
    juce::String logMsg = juce::String::formatted("NetworkStreamer: Intentando iniciar servidor en puerto %d (bindAddress: 0.0.0.0 - todas las interfaces)", serverPort);
    debugLog(logMsg);
    
    bool listenerCreated = false;
    
    // Intentar primero con "0.0.0.0" explícitamente (todas las interfaces)
    listenerCreated = serverSocket->createListener(serverPort, "0.0.0.0");
    
    // Si falla, intentar con string vacío (algunas versiones de JUCE lo interpretan como todas las interfaces)
    if (!listenerCreated)
    {
        debugLog(U8("NetworkStreamer: Fall\xc3\xb3 con 0.0.0.0, intentando con string vac\xc3\xado..."));
        delete serverSocket;
        serverSocket = new juce::StreamingSocket();
        listenerCreated = serverSocket->createListener(serverPort, "");
    }
    
    // Si aún falla, intentar con localhost (solo para debugging local)
    if (!listenerCreated)
    {
        debugLog(U8("NetworkStreamer: Fall\xc3\xb3 con todas las interfaces, intentando con localhost (solo local)..."));
        delete serverSocket;
        serverSocket = new juce::StreamingSocket();
        listenerCreated = serverSocket->createListener(serverPort, "127.0.0.1");
    }
    
    if (listenerCreated)
    {
        serverActive.store(true);
        shouldStop.store(false);
        
        juce::String successMsg = juce::String::formatted("NetworkStreamer: Servidor iniciado correctamente en puerto %d", serverPort);
        debugLog(successMsg);
        
        // Iniciar thread
        startThread();
        
        // Dar un momento para que el thread se inicie
        juce::Thread::sleep(100);
        
        return true;
    }
    else
    {
        // Error crítico: siempre loguear
        juce::String errorMsg = juce::String::formatted("NetworkStreamer: ERROR - No se pudo iniciar servidor en puerto %d. El puerto puede estar ocupado.", serverPort);
        juce::Logger::writeToLog(errorMsg);
        
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
    
    // Detener todos los ClientWorker threads
    {
        const juce::ScopedLock lock(clientsLock);
        for (auto& client : clients)
        {
            if (client != nullptr)
            {
                client->stopStreaming();
            }
        }
    }
    
    // Cerrar socket para despertar el thread
    if (serverSocket != nullptr)
    {
        serverSocket->close();
        delete serverSocket;
        serverSocket = nullptr;
    }
    
    // Esperar a que el thread termine
    stopThread(2000);
    
    // Esperar a que todos los ClientWorker threads terminen
    {
        const juce::ScopedLock lock(clientsLock);
        for (auto& client : clients)
        {
            if (client != nullptr)
            {
                client->stopThread(1000);
            }
        }
        clients.clear();
    }
    
    // Limpiar
    serverActive.store(false);
}

//==============================================================================
int NetworkStreamer::getNumClients() const
{
    const juce::ScopedLock lock(clientsLock);
    // Contar solo clientes activos
    int count = 0;
    for (const auto& client : clients)
    {
        if (client != nullptr && client->isClientActive())
            count++;
    }
    // Log periódico para debugging (solo cada 10 llamadas para no saturar)
    static int callCount = 0;
    if (ENABLE_DEBUG_LOGS && (++callCount % 10 == 0))
    {
        juce::String logMsg = juce::String::formatted("NetworkStreamer: getNumClients() = %d (total en lista: %d)", 
                                                       count, (int)clients.size());
        debugLog(logMsg);
    }
    return count;
}

//==============================================================================
std::vector<StreamingClient> NetworkStreamer::getClients() const
{
    const juce::ScopedLock lock(clientsLock);
    
    std::vector<StreamingClient> result;
    for (const auto& client : clients)
    {
        if (client != nullptr && client->isClientActive())
            result.push_back(client->getClientInfo());
    }
    return result;
}

//==============================================================================
void NetworkStreamer::cleanupFinishedWorkers()
{
    const juce::ScopedLock lock(clientsLock);
    
    // Remover threads que ya terminaron
    clients.erase(
        std::remove_if(clients.begin(), clients.end(),
            [](const std::unique_ptr<ClientWorker>& worker) {
                return worker != nullptr && !worker->isThreadRunning();
            }),
        clients.end()
    );
}

//==============================================================================
void NetworkStreamer::run()
{
    debugLog("NetworkStreamer: Thread started, waiting for connections...");
    
    // Verificar que el socket esté disponible
    if (serverSocket == nullptr)
    {
        juce::String errorMsg("NetworkStreamer: ERROR - serverSocket es nullptr en run()");
        juce::Logger::writeToLog(errorMsg);
        return;
    }
    
    juce::String readyMsg = juce::String::formatted("NetworkStreamer: Socket listo, esperando conexiones en puerto %d", serverPort);
    debugLog(readyMsg);
    
    int connectionCount = 0;
    
    while (!threadShouldExit() && !shouldStop.load())
    {
        if (serverSocket == nullptr)
        {
            juce::Thread::sleep(100);
            continue;
        }
        
        // Limpiar threads terminados periódicamente (cada 10 conexiones)
        if (connectionCount % 10 == 0)
        {
            cleanupFinishedWorkers();
        }
        
        // waitForNextConnection() bloquea indefinidamente hasta que llegue una conexión
        juce::StreamingSocket* clientSocket = serverSocket->waitForNextConnection();
        
        // Verificar si debemos detenernos
        if (threadShouldExit() || shouldStop.load())
        {
            if (clientSocket != nullptr)
            {
                clientSocket->close();
                delete clientSocket;
            }
            break;
        }
        
        if (clientSocket != nullptr && clientSocket->isConnected())
        {
            // Crear nuevo ClientWorker para manejar esta conexión
            auto worker = std::make_unique<ClientWorker>(clientSocket, 
                                                         audioBufferManager, 
                                                         synchronizationEngine);
            
            // Agregar a la lista de clientes
            {
                const juce::ScopedLock lock(clientsLock);
                clients.push_back(std::move(worker));
            }
            
            // Iniciar el thread del worker
            ClientWorker* workerPtr = clients.back().get();
            workerPtr->startThread();
            
            connectionCount++;
            
            juce::String connMsg = juce::String::formatted("NetworkStreamer: Nueva conexion aceptada (total: %d)", 
                                                           (int)clients.size());
            debugLog(connMsg);
        }
        else
        {
            // Socket inválido o no conectado, cerrarlo
            if (clientSocket != nullptr)
            {
                clientSocket->close();
                delete clientSocket;
            }
            
            // Verificar si el servidor aún está activo
            if (!serverActive.load())
            {
                debugLog(U8("NetworkStreamer: waitForNextConnection retorn\xc3\xb3 socket inv\xc3\xa1lido y serverActive es false"));
                break;
            }
        }
    }
    
    debugLog("NetworkStreamer: Thread stopped.");
}

//==============================================================================
void NetworkStreamer::debugLog(const juce::String& message) const
{
    if (ENABLE_DEBUG_LOGS)
    {
        juce::Logger::writeToLog(message);
    }
}

//==============================================================================
//==============================================================================
