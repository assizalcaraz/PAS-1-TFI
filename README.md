# streamAula - Plugin de Streaming de Audio en Tiempo Real

**Trabajo Final Integrador**  
**Programación Aplicada al Sonido (PAS)**  
**Especialización en Sonido para las Artes Digitales**  
**Universidad Nacional de las Artes (UNA)**

---

## Demo

[Mira el funcionamiento del plugin](https://youtu.be/fs5obFyNPYs)

---

## Descripción

streamAula es un plugin de audio desarrollado en C++ utilizando el framework JUCE que permite el streaming de audio en tiempo real desde un DAW hacia múltiples clientes conectados a través de la red local.

El plugin funciona como una herramienta de captura y transmisión de audio del aula hacia estudiantes remotos, sin necesidad de hardware adicional. El audio del DAW se captura en el plugin y se transmite via HTTP a navegadores web.

---

## Características

- **Plugin VST3/AU/AAX** para integración con DAWs (Reaper, Logic Pro, etc.)
- **Servidor HTTP embebido** - el plugin sirve su propia interfaz web
- **Streaming a múltiples clientes** simultáneos
- **Gestión lock-free de buffers** de audio (`juce::AbstractFifo`, un escritor y un lector + fan-out a N clientes)
- **Sincronización sample-accurate** basada en contador de samples
- **Interfaz web** con reproductor de audio integrado

---

## Arquitectura

```
DAW (pista de audio)
       │
       ▼
PluginProcessor::processBlock()  ← captura audio
       │
       ▼
AudioBufferManager (buffer circular lock-free, SPSC)
       │
       ▼
NetworkStreamer (servidor HTTP :8080 + hilo fan-out único lector)
       │
       ▼
Colas PCM por cliente → sockets `/stream`
       │
       ├── /       → Interfaz web con reproductor
       ├── /stream → Stream de audio PCM
       └── /config → Configuración JSON
```

### Componentes Principales

| Componente | Descripción |
|------------|-------------|
| `AudioBufferManager` | Buffer circular lock-free para gestión de audio |
| `NetworkStreamer` | Servidor HTTP embebido para streaming |
| `SynchronizationEngine` | Motor de sincronización sample-accurate |
| `PluginProcessor` | Procesador principal del plugin |

---

## Decisiones de Diseño

### Buffer circular lock-free (SPSC + fan-out)
El audio thread no puede bloquearse. `juce::AbstractFifo` enlaza **un** hilo escritor (`processBlock`) con **un** hilo lector (broadcast). Ese lector replica PCM hacia colas por cliente; cada `ClientWorker` solo escribe al socket, evitando múltiples lectores sobre el mismo FIFO.

### Servidor HTTP Embebido
El servidor HTTP vive dentro del plugin, eliminando dependencias externas y haciendo el sistema autocontenido.

### Timestamps Basados en Sample Count
La sincronización usa contador de samples en lugar de wall-clock, garantizando precisión a nivel de muestra de audio.

---

## Requisitos

- **JUCE Framework** 8.0+
- **Xcode** (macOS) o **Visual Studio** (Windows)
- **Projucer** (incluido con JUCE)
- **DAW** compatible con VST3/AU (Reaper recomendado)

---

## Compilación e Instalación

### 1. Abrir el proyecto
```bash
# Abrir en Projucer
open streamAula/streamAula.jucer
```

### 2. Configurar rutas de módulos JUCE
En Projucer, verificar que las rutas a los módulos JUCE apunten a la instalación local (por defecto `/Applications/JUCE/modules`).

### 3. Generar proyecto
```
File → Export → Xcode
```

### 4. Compilar en Xcode
- Seleccionar destino (MacOSX)
- Compilar en modo Release
- El plugin se genera en `Builds/MacOSX/build/`

### 5. Instalar el plugin

**VST3:**
```bash
cp -r Builds/MacOSX/build/streamAula.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

**Audio Unit:**
```bash
cp -r Builds/MacOSX/build/streamAula.component ~/Library/Audio/Plug-Ins/Components/
```

### 6. Usar en el DAW
1. Abrir el DAW (Reaper recomendado)
2. Agregar el plugin "streamAula" en una pista de audio
3. Asegurar que hay audio en la pista
4. Conectar clientes web

---

## Uso

### Desde el DAW
1. Cargar el plugin en una pista de audio
2. El servidor HTTP arranca automáticamente en puerto 8080
3. Verificar en la consola que el servidor está activo

### Desde el navegador web
1. Abrir `http://localhost:8080` (o la IP del equipo donde corre el plugin)
2. Hacer clic en "Iniciar Streaming"
3. El audio del DAW debería comenzar a reproducirse

---

## Especificaciones Técnicas

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

## Repositorio

- **GitHub**: https://github.com/assizalcaraz/PAS_TFI

---

*Trabajo Final Integrador - PAS - UNA - 2026*