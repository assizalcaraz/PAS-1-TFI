# streamAula - Memoria del Trabajo Final Integrador

**Programación Aplicada al Sonido (PAS)**  
**Especialización en Sonido para las Artes Digitales**  
**Universidad Nacional de las Artes (UNA)**  
**Autor**: José Assiz Alcaraz Baxter  
**Fecha**: Abril 2026

---

## 1. Resumen Ejecutivo

streamAula es un plugin de audio desarrollado en C++ utilizando el framework JUCE que permite el streaming de audio en tiempo real desde un DAW hacia múltiples clientes conectados a través de la red local.

El plugin captura el audio de entrada desde el DAW y lo transmite mediante HTTP a navegadores web, permitiendo que estudiantes oyentes reciban la señal de audio del aula sin necesidad de hardware adicional.

---

## 2. Funcionamiento del Software

### 2.1 Flujo de Audio

```
DAW (pista de audio)
    → PluginProcessor::processBlock()
    → AudioBufferManager (AbstractFifo: un escritor = audio thread)
    → Hilo AudioBroadcast (único lector de readFromBuffer)
    → PerClientAudioQueue × N (copia PCM por cliente; backpressure por cola)
    → ClientWorker × N (solo socket HTTP chunked)
    → Clientes web (navegadores)
```

**Contrato SPSC:** `juce::AbstractFifo` permite exactamente **un** hilo que llama a `readFromBuffer`. Varios clientes no pueden leer el mismo FIFO; el fan-out anterior replica el mismo chunk de audio hacia cada cola de salida.

### 2.2 Componentes Principales

#### AudioBufferManager
Gestor de buffer circular lock-free usando `juce::AbstractFifo`:
- Escritura desde el audio thread sin bloqueo (único escritor)
- Lectura desde **un solo** hilo de broadcast (único lector), no desde cada `ClientWorker`
- Pre-reserva de memoria para evitar allocation en tiempo real en el camino nominal

**Especificaciones:**
- Tamaño: 72,000 samples (~1,5 segundos @ 48 kHz)
- Canales: hasta 2 (estéreo)
- Contador atómico de samples procesados

#### NetworkStreamer
Servidor HTTP embebido:
- Thread de aceptación de conexiones (`NetworkStreamer::run`)
- Thread **AudioBroadcast**: única lectura del `AudioBufferManager` y distribución a colas PCM por cliente
- `ClientWorker`: HTTP + consumo de su cola y escritura chunked al socket
- Puerto: 8080 (configurable)
- Endpoints:
  - `/` - Interfaz web con reproductor
  - `/stream` - Streaming continuo de audio
  - `/config` - Configuración en JSON

#### SynchronizationEngine
Motor de sincronización basado en sample count (endpoint `/sync`, metadatos). **No forma parte del pipeline de bytes de `/stream`**: el audio HTTP sale del fan-out descrito arriba. `getSynchronizedChunk` queda disponible para extensiones futuras (p. ej. alineación multi-sala).

---

## 3. Decisiones de Diseño

### 3.1 Buffer Circular Lock-Free
**Problema**: El audio thread no puede bloquearse. Cualquier lock en `processBlock()` puede causar dropout.

**Solución**: Usar `juce::AbstractFifo` con **un escritor** (audio thread) y **un lector** (hilo broadcast). Entre lectura y navegadores se inserta fan-out con colas por cliente (fuera del hilo de audio).

### 3.2 Servidor HTTP Embebido
El servidor HTTP vive dentro del plugin, eliminando dependencias externas y haciendo el sistema autocontenido.

### 3.3 Sample-Count vs Wall-Clock
Los timestamps de sincronización usan sample count, no tiempo absoluto, garantizando precisión a nivel de muestra de audio.

### 3.4 Formato de Audio
PCM 16-bit little-endian interleaved sobre HTTP chunked, garantizando compatibilidad amplia con clientes web sin necesidad de códecs adicionales.

---

## 4. Estado del Proyecto

### Completado
- ✅ AudioBufferManager (lock-free, SPSC + fan-out hacia N clientes)
- ✅ Captura DAW básica
- ✅ NetworkStreamer (servidor HTTP, hilo broadcast, colas PCM por cliente)
- ✅ SynchronizationEngine base (metadatos `/sync`; flujo `/stream` documentado)
- ✅ Compilación (VST3, AU, Standalone)
- ✅ Interfaz web con reproductor
- ✅ Editor del plugin: ocupación del buffer según muestras listas para streaming (no `freeSpace` del FIFO); contador de clientes coherente (`isThreadRunning()` + `startThread` bajo `clientsLock`) y pacing del broadcast adaptativo si el FIFO acumula backlog

### Pendientes (no críticos)
- OPUS encoder (compresión)
- Métricas de latencia documentadas

---

## 5. Especificaciones Técnicas

| Aspecto | Valor |
|---------|-------|
| Framework | JUCE 8.0.10 |
| Compilador | Xcode (macOS) |
| Targets | VST3, AU, Standalone |
| Buffer circular | 72,000 samples (~1,5 s @ 48 kHz) |
| Puerto HTTP | 8080 |
| Canales | Estéreo |
| Formato salida | PCM 16-bit |
| Latencia objetivo | <20ms |

**Nota (tasa de muestreo en el editor del plugin):** el valor mostrado es la tasa que el **host (DAW o standalone)** entrega en `prepareToPlay`. Si aparece 8000 Hz, 16000 Hz, etc., el proyecto o el **dispositivo de entrada/salida** está configurado así (p. ej. utilitario **Audio MIDI Setup** en macOS, o el dispositivo elegido en Reaper/Logic). El plugin no fuerza 48 kHz por sí mismo; hay que subir la tasa en la configuración de audio del host.

---

## 6. Revisión del Ciclo de Audio

El método `processBlock()` sigue las mejores prácticas de JUCE:

```cpp
void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;  // Evita CPU spikes por denormals
    
    // Limpia outputs extra
    for (auto i = inputChannels; i < outputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    
    // Passthrough + escritura al buffer circular (lock-free)
    if (audioBufferManager != nullptr) {
        audioBufferManager->writeToBuffer(buffer);
    }
}
```

**Características:**
- `ScopedNoDenormals` para evitar problemas de denormals
- Pre-reserva de memoria (ninguna allocation en runtime)
- Escritura lock-free mediante AbstractFifo
- Lógica mínima en el audio thread

---

---

*Trabajo Final Integrador - PAS - UNA - 2026*