# streamAula - Arquitectura P2P de Audio

## Visión General

streamAula es un plugin de audio que funciona como servidor de streaming + sistema P2P para distribución de audio sincronizado a múltiples clientes sin costo de ancho de banda en el servidor.

## Problema a Resolver

- **Escenario actual**: Un servidor envía audio a N clientes = N conexiones = cuello de banda
- **Solución P2P**: Los clientes re-transmiten audio entre sí, reduciendo la carga en el servidor

## Arquitectura Propuesta

```
[Plugin (Maestro)] 
       │
       ├─→ [Cliente 1] (re-transmisor)
       │         ├─→ [Cliente 2]
       │         └─→ [Cliente 3]
       │
       ├─→ [Cliente 4] (re-transmisor)
       │         ├─→ [Cliente 5]
       │         └─→ [Cliente 6]
       │
       └─→ [Cliente 7...N] (solo reciben)
```

### Roles de Nodos

1. **Maestro**: El plugin que genera el audio
2. **Re-transmisor**: Cliente seleccionado para re-transmitir a otros
3. **Hoja**: Cliente que solo recibe

El servidor (plugin) asigna automáticamente los roles según:
- Cantidad de clientes conectados
- Latencia de red de cada cliente
- Capacidad de procesamiento (estimada por tasa deACKs)

## Protocolo de Señalamiento

### Conexión Inicial

```
1. Cliente → Servidor: GET /stream
2. Servidor → Cliente: 200 OK + metadata (configuración)
3. Servidor: Registra cliente, asigna rol (re-transmisor o hoja)
```

### Descubrimiento de Red P2P

El servidor envía a cada cliente información de conexión:

```json
{
  "role": "relay",
  "parent": "cliente_3",
  "children": ["cliente_2", "cliente_5"],
  "audioBuffer": "ws://cliente_3:8081/stream"
}
```

### Comunicación P2P

- **WebRTC** para conexiones P2P reales entre clientes
- **DTLS** para encryption (TLS 1.3)
- **SRTP** para streaming de audio encriptado

## Seguridad

### Autenticación
- QR包含 token único por sesión
- Servidor valida token antes deaceptar conexión

### Encryption
- Todas las comunicaciones P2P usan DTLS (obligatorio)
- Integridad de chunks via sequence number

### Aislamiento
- Clientes solo pueden conectarse a nodos designados por servidor
- No hay connections directas no autorizadas

## Casos de Uso

### 1. Clase con 30 estudiantes
```
[Plugin Profesor] 
       │
       ├─→ [PC Estudiante 1] (re-transmisor) → 5 estudiantes
       ├─→ [PC Estudiante 2] (re-transmisor) → 5 estudiantes
       ├─→ [PC Estudiante 3] (re-transmisor) → 5 estudiantes
       ...y así sucesivamente
```

### 2. Fiesta Silente (múltiples salas)
```
[Plugin - Sala Rock]   → Grupo Rock
[Plugin - Sala Salsa]  → Grupo Salsa
[Plugin - Salsa]     → Grupo Electrónica
```

Cada sala es una instancia independiente del servidor.

## Installer

El plugin se distribuye como:
- **Mac**: `.pkg` o `.vst3` dentro de `.zip`
- **Windows**: `.exe` installer (Inno Setup)

El usuario solo necesita:
1. Descargar archivo
2. Ejecutar installer
3. "Siguiente, aceptar, siguiente, finalizar"
4. Abrir DAW y cargar plugin

## Métricas de Evaluación

- Latencia de red (ms)
- Ancho de banda del servidor vs clientes
- Jitter de audio
- Tiempo de sincronización entre clientes
- Cantidad máximo de clientes por nodo re-transmisor

## Decisiones Técnicas Pendientes

- Topology del árbol (binario, balanceado, mesh parcial)
- Cantidad óptima de clientes por re-transmisor
- Protocolo de reconexión en caso de desconexión
- Buffer size para reconversión de jitter

## Referencias

- WebRTC: https://webrtc.org/
- DTLS: RFC 6347
- SRTP: RFC 3711