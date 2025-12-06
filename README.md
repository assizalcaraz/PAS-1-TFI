# streamAula - Plugin JUCE para Streaming de Audio

**Proyecto**: Trabajo Final - Programación Aplicada al Sonido  
**Posgrado**: Especialización en Sonido para las Artes Digitales  
**Universidad**: UNA (Universidad Nacional de las Artes)  
**Autor**: Assiz Alcaraz Baxter  
**GitHub**: https://github.com/assizalcaraz  
**Web**: https://www.assiz.ar

---

## 🎯 Objetivo

Desarrollar un **plugin de audio JUCE (C++)** que:

1. **Gestione eficientemente buffers de audio** con bajo overhead
2. **Implemente streaming sincronizado** para múltiples dispositivos
3. **Mantenga sincronización precisa** (<5ms diferencia entre clientes)
4. **Se integre nativamente con DAWs** (Reaper, etc.) como plugin VST3/AU
5. **Demuestre dominio de C++** y procesamiento de audio en tiempo real

---

## 📋 Relación con aula_digital

Este proyecto es la **refactorización en C++/JUCE** de la aplicación Python `aula_digital`.

- **Proyecto original**: [`aula_digital`](https://github.com/assizalcaraz/aula_digital) (Python/Flask)
- **Proyecto refactorizado**: `PAS_TFI/streamAula` (C++/JUCE)
- **Documentación de migración**: Ver [`aula_digital/docs/juce/`](https://github.com/assizalcaraz/aula_digital/tree/factorizacion_c%2B%2B/docs/juce)

---

## 🏗️ Estado Actual

### Proyecto Base
- ✅ Proyecto JUCE configurado (VST3/AU/AAX)
- ✅ Plugin básico compila y carga en DAW
- ✅ Estructura de archivos estándar

### Próximos Pasos
Ver [Roadmap de Migración](https://github.com/assizalcaraz/aula_digital/blob/factorizacion_c%2B%2B/docs/juce/ROADMAP_MIGRACION_C%2B%2B.md) en `aula_digital`.

---

## 📁 Estructura del Proyecto

```
PAS_TFI/
└── NewProject/
    ├── NewProject.jucer          # Archivo de configuración Projucer
    ├── Source/
    │   ├── PluginProcessor.h/cpp # Procesador de audio
    │   └── PluginEditor.h/cpp    # Interfaz de usuario
    ├── JuceLibraryCode/          # Código de JUCE (generado)
    └── Builds/
        └── MacOSX/               # Proyectos Xcode
```

---

## 🚀 Desarrollo

### Requisitos
- JUCE Framework (última versión)
- Projucer (JUCE Project Generator)
- Xcode (macOS) o Visual Studio (Windows)
- DAW para testing (Reaper recomendado)

### Compilar
1. Abrir `NewProject.jucer` en Projucer
2. Configurar módulos JUCE necesarios
3. Generar proyecto Xcode/Visual Studio
4. Compilar desde IDE

### Testing
1. Compilar plugin en modo Release
2. Copiar `.vst3` o `.component` a carpeta de plugins del sistema
3. Cargar en DAW (Reaper)
4. Verificar que plugin aparece y procesa audio

---

## 📚 Documentación

### Documentación de Migración (en aula_digital)
- [Roadmap de Migración](https://github.com/assizalcaraz/aula_digital/blob/factorizacion_c%2B%2B/docs/juce/ROADMAP_MIGRACION_C%2B%2B.md)
- [Análisis de Migración](https://github.com/assizalcaraz/aula_digital/blob/factorizacion_c%2B%2B/docs/juce/ANALISIS_MIGRACION_JUCE.md)
- [Recomendaciones Técnicas](https://github.com/assizalcaraz/aula_digital/blob/factorizacion_c%2B%2B/docs/juce/RECOMENDACIONES_JUCE.md)
- [Resumen Ejecutivo](https://github.com/assizalcaraz/aula_digital/blob/factorizacion_c%2B%2B/docs/juce/RESUMEN_EJECUTIVO_JUCE.md)

### Índice Completo
Ver [`aula_digital/docs/juce/INDICE_DOCUMENTACION_JUCE.md`](https://github.com/assizalcaraz/aula_digital/blob/factorizacion_c%2B%2B/docs/juce/INDICE_DOCUMENTACION_JUCE.md)

---

## 🗺️ Roadmap

### Fase 0: Setup ✅
- [x] Proyecto JUCE configurado
- [x] Plugin básico compila
- [x] Plugin carga en DAW

### Fase 1: AudioBufferManager (Semanas 2-3)
- [ ] Implementar `AudioBufferManager` con `juce::AbstractFifo`
- [ ] Pre-reservación de memoria
- [ ] Alineación SIMD
- [ ] Tests unitarios

### Fase 2: Captura DAW (Semana 4)
- [ ] Implementar `processBlock()`
- [ ] Passthrough correcto
- [ ] Escritura a buffer

### Fase 3: NetworkStreamer (Semanas 5-6)
- [ ] Servidor HTTP embebido
- [ ] Múltiples clientes
- [ ] Thread separado

### Fase 4: Sincronización (Semanas 7-8)
- [ ] Sample-accurate timing
- [ ] Compensación de latencia
- [ ] Métricas de precisión

### Fase 5: Codificación (Semana 9)
- [ ] OPUS encoder
- [ ] Fallback PCM

### Fase 6: UI (Semana 10)
- [ ] Plugin editor
- [ ] QR code
- [ ] Estadísticas

### Fase 7: Optimización (Semanas 11-12)
- [ ] Tests completos
- [ ] Métricas documentadas
- [ ] Presentación

**Ver roadmap completo**: [`aula_digital/docs/juce/ROADMAP_MIGRACION_C++.md`](https://github.com/assizalcaraz/aula_digital/blob/factorizacion_c%2B%2B/docs/juce/ROADMAP_MIGRACION_C%2B%2B.md)

---

## 📊 Métricas Objetivo

- **Latencia total**: <20ms (en red local)
- **Sincronización**: <5ms diferencia entre clientes
- **Escalabilidad**: 50+ clientes simultáneos
- **CPU**: <20% con 10 clientes, <50% con 50 clientes

---

## 🎓 Aspectos Académicos

1. **Gestión de Buffers**: Lock-free structures, memory alignment, anti-dropout
2. **Sincronización**: Sample-accurate timing, network compensation
3. **Tiempo Real**: Real-time constraints, thread separation
4. **Integración Profesional**: Plugin architecture, DAW integration

---

## 📝 Notas de Desarrollo

- Este proyecto se desarrolla en paralelo con `aula_digital`
- La documentación principal está en `aula_digital/docs/juce/`
- El código fuente se desarrolla en este proyecto (`PAS_TFI`)
- Commits y versiones se gestionan en este repositorio

---

**Última actualización**: 2025-12-06

