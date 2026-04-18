# Plan de Análisis y Mejora - streamAula

## Modelo SPSC y fan-out (2026-04)

`juce::AbstractFifo` en `AudioBufferManager` cumple el contrato **un escritor / un lector** (SPSC): el audio thread escribe con `writeToBuffer`; **solo un hilo** debe llamar a `readFromBuffer`.

Para N clientes HTTP no se puede leer N veces del mismo FIFO (no es broadcast: cada `finishedRead` consume muestras; varios lectores además provocan condiciones de carrera). La arquitectura correcta es **fan-out**:

1. Un hilo dedicado (`AudioBroadcastThread` en `NetworkStreamer.cpp`) lee un chunk del `AudioBufferManager` por tick.
2. Convierte a PCM int16 interleaved una vez.
3. Encola una **copia** por cliente en `PerClientAudioQueue` (backpressure: descarta los chunks más viejos si la cola del cliente supera un máximo).
4. Cada `ClientWorker` solo hace I/O de socket consumiendo su cola.

Documentación alineada: [`docs/MEMORIA_TFI.md`](MEMORIA_TFI.md) sección 2.1.  
Lista de comprobación manual: [`docs/PRUEBAS_STREAMING.md`](PRUEBAS_STREAMING.md).

### Interfaz del plugin y telemetría (2026-04)

- **Ocupación del buffer:** debe calcularse con `getAvailableSamples()` (datos pendientes para el lector de streaming). `getFreeSpace()` del `AbstractFifo` con cola casi vacía suele ser `bufferSize − 1` (p. ej. 71999), lo que hacía que `(bufferSize − freeSpace)` pareciera ~0 % fijo.
- **Clientes conectados:** `isClientActive()` debe basarse en `juce::Thread::isThreadRunning()` (API thread-safe de JUCE), no en un `bool` leído desde el timer del editor (condición de carrera). `push_back` + `startThread()` del worker deben hacerse bajo el mismo `clientsLock` para que el conteo no vea hilos aún no arrancados. `cleanupFinishedWorkers()` en cada ciclo del aceptador sigue retirando entradas ya terminadas.

---

## A) ANÁLISIS EXHAUSTIVO

### 1. ARQUITECTURA PROBLEMÁTICA (corregida en 2026-04)

| Problema | Ubicación | Impacto |
|----------|-----------|---------|
| ~~**Un thread por cliente**~~ | `ClientWorker` | Resuelto: un solo lector del FIFO + colas por cliente |
| ~~**Lectura competida**~~ | `readFromBuffer()` | Resuelto: lectura única en hilo broadcast |
| ~~**Sin sincronización entre clientes**~~ | — | Resuelto: mismas muestras replicadas a cada cola |

### 2. INEFICIENCIAS CRÍTICAS

| # | Problema | Archivo | Línea | Descripción |
|---|----------|--------|-------|-------------|
| 1 | **HeapBlock en cada chunk** | `NetworkStreamer.cpp` | 552 | `juce::HeapBlock<int16_t>` allocation dinámica en CADA chunk enviado (~1000x/segundo) |
| 2 | **makeCopyOf en Synchronization** | `SynchronizationEngine.cpp` | 187 | Copia completa del buffer en cada chunk |
| 3 | **UI Web ineficiente** | `app.js` | Múltiples | Buffer accumulation, parsing manual, conversiones múltiples |
| 4 | **String allocations en streaming** | `NetworkStreamer.cpp` | 555, 558 | Headers HTTP allocados en cada chunk |

### 3. MALAS PRÁCTICAS

| # | Práctica | Archivo | Descripción |
|---|---------|--------|-------------|
| 1 | `#include <signal.h>` | NetworkStreamer.cpp:12 | No portable a Windows |
| 2 | Múltiples `ENABLE_*_DEBUG_LOGS` |分散| Flags duplicados en vez de uno solo |
| 3 | Sleeps hardcodedados | NetworkStreamer.cpp | Sin consideraciones de sample rate dinámico |

### 4. FALLAS DE DISEÑO

| # | Falla | Impacto |
|---|-------|--------|
| 1 | **Arquitectura "push"** pero debería ser "pull" | El servidor empuja sin control de flujo real |
| 2 | No hay backpressure | Si el cliente lento, el buffer se vacía |
| 3 | No hay reconexión robusta | Hay que recargar el plugin |

---

## B) PARCHES A IMPLEMENTAR

### Priority 1: Estabilidad básica
- [ ] Eliminar HeapBlock en cada chunk (pre-allocate)
- [ ] Eliminar makeCopyOf de SynchronizationEngine
- [ ] Agregar pre-buffer en cliente web

### Priority 2: Multi-cliente
- [ ] Implementar sleep adaptativo basado en clientes activos
- [ ] Agregar throttling cuando buffer está bajo

### Priority 3: Reconexión
- [ ]Mejorar manejo de desconexión
- [ ] Auto-reconexión en cliente web

---

## C) DOCUMENTACIÓN DE CAMBIOS

Ver `docs/CHANGELOG.md` para tracking de cambios.

---

## ACCIONES INMEDIATAS

### 1. Pre-allocate buffer de conversión (Priority 1) ✅ IMPLEMENTADO
```cpp
// En ClientWorker - member field pre-allocated
juce::HeapBlock<int16_t> int16Buffer{1024}; // para chunk de 512 stereo
```

### 2. Eliminar SynchronizationEngine del flujo (Priority 1) ✅ YA HECHO
```cpp
// Usar AudioBufferManager directamente, sin SynchronizationEngine
```

### 3. Pre-buffer en cliente JS (Priority 1) ✅ IMPLEMENTADO
```javascript
const muestrasPorBuffer = 8192; // 170ms a 48kHz (antes 4096 = 85ms)
```

---

## CAMBIOS IMPLEMENTADOS (2026-04-18)

### v1.0.1 - Estabilidad

| # | Cambio | Descripción |
|---|--------|-----------|
| 1 | Pre-alloc int16 buffer | Elimina HeapBlock por chunk (ahorro ~1000 allocations/seg) |
| 2 | Aumentar buffer JS | 85ms → 170ms para mayor robustez |
| 3 | Simplificar bucle streaming | Sleep fijo 21ms para estabilidad |

*Documento generado: 2026-04-18*