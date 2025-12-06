# Solución de Problemas de Build

**Fecha**: 2025-12-06

---

## ✅ Problemas Resueltos

### 1. Warning: Variable no usada `channelData`

**Problema**: 
```
PluginProcessor.cpp:155:15 Unused variable 'channelData'
```

**Solución**: 
- Eliminada la variable no usada
- Agregado comentario TODO para futura integración con AudioBufferManager
- El código ahora hace passthrough simple sin warnings

---

### 2. Error: Bundle Identifier inválido para AAX

**Problema**:
```
invalid character in Bundle Identifier. This string must be a uniform type identifier (UTI) 
that contains only alphanumeric (A-Z,a-z,0-9), hyphen (-), and period (.) characters.
```

**Causa**: 
El `companyName` en el `.jucer` era "4ssiiz", lo que generaba `com._4ssiiz.streamAula` (con guion bajo antes del número, que es inválido).

**Solución**:
- Cambiado `companyName` de "4ssiiz" a "assiz" en `NewProject.jucer`
- Esto generará `com.assiz.streamAula` (válido)

**Acción requerida**:
1. Abrir `NewProject.jucer` en Projucer
2. Guardar (esto regenerará los archivos con el nuevo Bundle Identifier)
3. Recompilar el proyecto

---

### 3. Build exitoso pero no abre ventana

**Explicación**:
Los plugins de audio **NO son aplicaciones standalone** por defecto. Son bibliotecas que se cargan dentro de un DAW (Digital Audio Workstation).

**Opciones**:

#### Opción A: Cargar en un DAW (Recomendado para desarrollo)
1. Compilar el plugin (VST3 o AU)
2. Copiar el plugin a la carpeta de plugins del sistema:
   - **VST3**: `~/Library/Audio/Plug-Ins/VST3/`
   - **AU**: `~/Library/Audio/Plug-Ins/Components/`
3. Abrir Reaper (u otro DAW)
4. Agregar el plugin a una pista
5. Abrir la interfaz del plugin desde el DAW

#### Opción B: Configurar como Standalone (Para testing rápido)

**En Projucer**:
1. Abrir `NewProject.jucer`
2. En la sección **"Plugin is a Synth"** o **"Plugin Characteristics"**
3. Buscar la opción **"Plugin Wants Midi Input"** (si está disponible)
4. O mejor: En **Exporters** → **Xcode (MacOSX)**
5. Verificar que hay un target **"Standalone Plugin"** o similar
6. Si no existe, Projucer puede generar uno automáticamente

**Alternativa - Crear Standalone manualmente**:
1. En Projucer, ir a **File** → **New Project** → **Audio Application**
2. O modificar el proyecto actual para incluir un target standalone

**Nota**: Para el trabajo final, probablemente NO necesites standalone. Es más útil cargar el plugin en Reaper para demostrar la integración con DAW.

---

## 🔧 Pasos para Regenerar Proyecto

1. **Abrir Projucer**:
   ```bash
   # Si tienes Projucer instalado
   open /Applications/Projucer.app
   # O desde Downloads si lo descargaste
   open ~/Downloads/JUCE/Projucer.app
   ```

2. **Abrir proyecto**:
   - File → Open → Seleccionar `NewProject.jucer`

3. **Verificar configuración**:
   - Company Name: `assiz` (debe estar corregido)
   - Plugin Name: `streamAula`
   - Plugin Formats: VST3, AU, AAX (según necesites)

4. **Guardar y regenerar**:
   - File → Save Project
   - Esto regenerará los archivos Xcode con el Bundle Identifier correcto

5. **Recompilar**:
   - Abrir el proyecto Xcode generado
   - Product → Clean Build Folder (⇧⌘K)
   - Product → Build (⌘B)

---

## 📍 Ubicación de Plugins Compilados

Después de compilar, los plugins estarán en:

```
NewProject/Builds/MacOSX/build/Debug/
├── NewProject.vst3/          # Plugin VST3
├── NewProject.component/     # Plugin AU
└── NewProject.aaxplugin/     # Plugin AAX (si compilaste)
```

**Para usar en Reaper**:
1. Copiar `.vst3` o `.component` a la carpeta de plugins
2. Reiniciar Reaper
3. El plugin aparecerá en la lista de plugins

---

## ✅ Checklist Post-Build

- [ ] Bundle Identifier corregido (com.assiz.streamAula)
- [ ] Warning de variable no usada eliminado
- [ ] Proyecto compila sin errores
- [ ] Plugin se puede cargar en Reaper (o DAW de prueba)
- [ ] Interfaz del plugin se abre correctamente

---

## 🎯 Próximos Pasos

1. **Regenerar proyecto desde Projucer** (para aplicar Bundle Identifier)
2. **Compilar y verificar** que no hay errores
3. **Cargar en Reaper** para testing
4. **Integrar AudioBufferManager** con PluginProcessor (Fase 1 - siguiente paso)

---

**Última actualización**: 2025-12-06

