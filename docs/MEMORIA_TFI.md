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
    → AudioBufferManager (buffer circular lock-free)
    → NetworkStreamer (servidor HTTP)
    → Clientes web (navegadores)
```

### 2.2 Componentes Principales

#### AudioBufferManager
Gestor de buffer circular lock-free usando `juce::AbstractFifo`:
- Escritura desde el audio thread sin bloqueo
- Lectura desde el network thread sin locks
- Pre-reserva de memoria para evitar allocation en tiempo real

**Especificaciones:**
- Tamaño: 144,000 samples (~3 segundos @ 48kHz)
- Canales: hasta 2 (estéreo)
- Contador atómico de samples procesados

#### NetworkStreamer
Servidor HTTP embebido que corre en thread separado:
- Puerto: 8080 (configurable)
- Endpoints:
  - `/` - Interfaz web con reproductor
  - `/stream` - Streaming continuo de audio
  - `/config` - Configuración en JSON

#### SynchronizationEngine
Motor de sincronización basado en sample count:
- Timestamps basados en contador de samples
- Buffer de sincronización para nuevos clientes
- Compensación de latencia adaptativa

---

## 3. Decisiones de Diseño

### 3.1 Buffer Circular Lock-Free
**Problema**: El audio thread no puede bloquearse. Cualquier lock en `processBlock()` puede causar dropout.

**Solución**: Usar `juce::AbstractFifo` para permitir escritura y lectura concurrentes sin locks.

### 3.2 Servidor HTTP Embebido
El servidor HTTP vive dentro del plugin, eliminando dependencias externas y haciendo el sistema autocontenido.

### 3.3 Sample-Count vs Wall-Clock
Los timestamps de sincronización usan sample count, no tiempo absoluto, garantizando precisión a nivel de muestra de audio.

### 3.4 Formato de Audio
PCM 16-bit little-endian interleaved sobre HTTP chunked, garantizando compatibilidad amplia con clientes web sin necesidad de códecs adicionales.

---

## 4. Estado del Proyecto

### Completado
- ✅ AudioBufferManager (lock-free)
- ✅ Captura DAW básica
- ✅ NetworkStreamer (servidor HTTP, múltiples clientes)
- ✅ SynchronizationEngine base
- ✅ Compilación (VST3, AU, Standalone)
- ✅ Interfaz web con reproductor

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
| Buffer | 144,000 samples (~3s @ 48kHz) |
| Puerto HTTP | 8080 |
| Canales | Estéreo |
| Formato salida | PCM 16-bit |
| Latencia objetivo | <20ms |

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

## 7. Entregables

1. **Plugin funcional**: Compilado como VST3/AU
2. **Código fuente**: Proyecto JUCE completo
3. **Esta memoria**: Descripción del instrumento y decisiones de diseño
4. **Video demostrativo**: (requiere captura de pantalla)

---

*Trabajo Final Integrador - PAS - UNA - 2026*