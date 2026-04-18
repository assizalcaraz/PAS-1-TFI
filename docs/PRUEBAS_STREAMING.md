# Protocolo de pruebas — streaming multi-cliente (streamAula)

Objetivo: validar el modelo **un lector del `AudioBufferManager` + fan-out** y la estabilidad del hilo de audio tras los parches.

## Preparación

1. Compilar el plugin (Standalone o VST3) y cargar una pista con señal estable (tono, música, micrófono).
2. Iniciar reproducción en el DAW para que `processBlock` alimente el buffer.
3. Anotar la IP mostrada en `/config` si se prueba desde otra máquina en LAN.

## Casos obligatorios

### A — Un cliente

1. Abrir `http://<host>:8080/` y pulsar reproducir / iniciar stream.
2. **Esperado:** audio continuo, sin saltos regulares; en consola del host (debug) sin ráfagas de “buffer lleno”.

### B — Dos o tres clientes simultáneos

1. Abrir dos o tres pestañas o dispositivos contra el mismo `http://<host>:8080/` y conectar el stream en todos.
2. **Esperado:** cada cliente oye la **misma** señal (mismo contenido en el mismo instante, salvo latencia de red); no debe sonar como “mitad de muestras” ni como canales intercalados entre clientes.
3. **Esperado:** el FIFO no se vacía de forma anómala solo por existir más de un cliente (antes del fan-out sí ocurría competencia de lectores).

### C — Desconexión de un cliente

1. Con dos clientes activos, cerrar una pestaña o cortar la red de uno.
2. **Esperado:** el cliente restante sigue recibiendo audio sin quedar colgado indefinidamente en `popChunk`.

### E — Contador en el plugin (host)

1. Con varias pestañas en `/stream`, observar en la ventana del plugin **“Clientes conectados”**.
2. Cerrar una o más pestañas.
3. **Esperado:** el número baja acorde a las desconexiones (el conteo usa el estado real del hilo `ClientWorker`, no un `bool` compartido sin sincronización con el timer del editor).

### D — Throttling (cliente lento)

1. En Chrome DevTools → Network → throttling “Slow 3G” en **una** pestaña; la otra sin throttle.
2. **Esperado:** la pestaña lenta puede perder datos (cola con `maxQueuedChunks` y descarte del más viejo), pero el plugin no debe corromper PCM ni tumbar al otro cliente. El cliente lento puede acumular latencia o gaps.

## Regresión AudioBufferManager (RT)

- Forzar situación de buffer lleno con entrada en silencio (si aplica al escenario) y verificar en profiler/logs que **no** se llama a `setSize` en el hilo de audio en el branch de descarte (reuso de `tempBuffer`).

## Opcional (automatización)

- Si en el futuro se añade target de tests JUCE: hilo que escribe bloques conocidos en `AudioBufferManager`, hilo broadcast que lee y dos colas mock verifican mismas muestras int16.

---

*Documento alineado con el plan de fan-out — abril 2026.*
