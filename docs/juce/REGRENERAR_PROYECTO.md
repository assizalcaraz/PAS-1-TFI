# Instrucciones: Regenerar Proyecto Xcode

**Fecha**: 2025-12-06  
**Problema**: `NetworkStreamer.cpp` no está en el proyecto Xcode (undefined symbols)

---

## ⚠️ Problema

El archivo `NetworkStreamer.cpp` está en el `.jucer` pero **NO** está en el proyecto Xcode generado. Esto causa errores de "undefined symbols".

---

## ✅ Solución: Regenerar Proyecto

### Paso 1: Abrir Projucer

```bash
# Opción 1: Si Projucer está en Applications
open /Applications/Projucer.app

# Opción 2: Si está en Downloads
open ~/Downloads/JUCE/Projucer.app

# Opción 3: Buscar Projucer
find ~ -name "Projucer.app" 2>/dev/null | head -1 | xargs open
```

### Paso 2: Abrir Proyecto

1. En Projucer: **File → Open**
2. Navegar a: `/Users/joseassizalcarazbaxter/Documents/UNA/POSGRADO/2025_2/PA1/PAS_TFI/streamAula/`
3. Seleccionar: `streamAula.jucer`
4. Abrir

### Paso 3: Verificar Archivos

En Projucer, en la lista de archivos del grupo "Source", verificar que aparecen:

- ✅ `PluginProcessor.cpp` / `PluginProcessor.h`
- ✅ `PluginEditor.cpp` / `PluginEditor.h`
- ✅ `AudioBufferManager.cpp` / `AudioBufferManager.h`
- ✅ `NetworkStreamer.cpp` / `NetworkStreamer.h` ← **Verificar que está aquí**

### Paso 4: Guardar Proyecto

1. **File → Save Project** (o ⌘S)
2. Esto regenerará el proyecto Xcode con todos los archivos

### Paso 5: Limpiar y Recompilar en Xcode

1. **Abrir Xcode**:
   ```bash
   open streamAula/Builds/MacOSX/streamAula.xcodeproj
   ```

2. **Limpiar Build**:
   - Product → Clean Build Folder (⇧⌘K)

3. **Verificar archivos**:
   - En el navegador de proyectos (lado izquierdo)
   - Expandir "Source"
   - Verificar que `NetworkStreamer.cpp` aparece

4. **Verificar Build Phases**:
   - Seleccionar target "streamAula - Shared Code"
   - Build Phases → Compile Sources
   - Verificar que `NetworkStreamer.cpp` está en la lista

5. **Compilar**:
   - Product → Build (⌘B)
   - Verificar que no hay errores de linking

---

## 🔍 Si NetworkStreamer.cpp No Aparece en Projucer

Si después de abrir el `.jucer` no ves `NetworkStreamer.cpp`:

1. **Agregar manualmente en Projucer**:
   - Click derecho en grupo "Source"
   - Add Existing Files...
   - Seleccionar `Source/NetworkStreamer.cpp` y `Source/NetworkStreamer.h`
   - Asegurar que `NetworkStreamer.cpp` tiene "compile" marcado

2. **Guardar proyecto** (⌘S)

3. **Regenerar Xcode** (automático al guardar)

---

## ✅ Verificación Final

Después de regenerar, verificar:

1. ✅ `NetworkStreamer.cpp` aparece en Xcode
2. ✅ Está en "Compile Sources" del target
3. ✅ Compila sin errores de undefined symbols
4. ✅ Warning de Font corregido

---

## 📝 Nota sobre Font Warning

El warning de Font ya está corregido usando:
```cpp
juce::Font titleFont = juce::Font(juce::FontOptions(24.0f).withStyle(juce::Font::bold));
```

---

**Última actualización**: 2025-12-06

