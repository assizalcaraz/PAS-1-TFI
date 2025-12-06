# Errores de Compilación - Soluciones

**Fecha**: 2025-12-06

---

## ❌ Errores Actuales

### 1. Font Warning (Corregido)
**Error**: `No viable conversion from 'juce::Font::FontStyleFlags' to 'String'`

**Solución aplicada**:
```cpp
// Antes (incorrecto):
juce::Font titleFont = juce::Font(juce::FontOptions(24.0f).withStyle(juce::Font::bold));

// Ahora (correcto):
juce::Font titleFont(24.0f);
titleFont = titleFont.boldened();
```

---

### 2. Undefined Symbols de NetworkStreamer

**Errores**:
```
Undefined symbol: NetworkStreamer::stopServer()
Undefined symbol: NetworkStreamer::startServer()
Undefined symbol: NetworkStreamer::NetworkStreamer(AudioBufferManager*, int)
Undefined symbol: NetworkStreamer::getNumClients() const
```

**Causa**: `NetworkStreamer.cpp` **NO está en el proyecto Xcode**, aunque está en el `.jucer`.

**Solución**: **Regenerar proyecto en Projucer**

---

## ✅ Pasos para Resolver

### 1. Regenerar Proyecto en Projucer

**IMPORTANTE**: Debes regenerar el proyecto Xcode desde Projucer para que incluya `NetworkStreamer.cpp`.

1. Abrir `streamAula.jucer` en Projucer
2. Verificar que `NetworkStreamer.cpp` y `NetworkStreamer.h` aparecen en la lista
3. **File → Save Project** (esto regenera Xcode)
4. En Xcode: **Product → Clean Build Folder** (⇧⌘K)
5. En Xcode: **Product → Build** (⌘B)

### 2. Verificar en Xcode

Después de regenerar, verificar:

1. **Navegador de proyectos**:
   - Expandir "Source"
   - Verificar que `NetworkStreamer.cpp` aparece

2. **Build Phases**:
   - Seleccionar target "streamAula - Shared Code"
   - Build Phases → Compile Sources
   - Verificar que `NetworkStreamer.cpp` está en la lista

---

## 🔧 Si NetworkStreamer.cpp No Aparece Después de Regenerar

Si después de regenerar en Projucer, `NetworkStreamer.cpp` aún no aparece en Xcode:

### Opción A: Agregar Manualmente en Projucer

1. En Projucer, click derecho en grupo "Source"
2. **Add Existing Files...**
3. Seleccionar `Source/NetworkStreamer.cpp` y `Source/NetworkStreamer.h`
4. Asegurar que `NetworkStreamer.cpp` tiene checkbox "compile" marcado
5. Guardar proyecto

### Opción B: Agregar Manualmente en Xcode (No recomendado)

1. En Xcode, click derecho en grupo "Source"
2. **Add Files to "streamAula"...**
3. Seleccionar `NetworkStreamer.cpp` y `NetworkStreamer.h`
4. Asegurar que está en el target correcto
5. **Nota**: Esto puede perderse al regenerar desde Projucer

---

## 📋 Checklist de Verificación

- [ ] Projucer abierto con `streamAula.jucer`
- [ ] `NetworkStreamer.cpp` y `.h` aparecen en la lista de archivos
- [ ] Proyecto guardado en Projucer
- [ ] Xcode abierto con proyecto regenerado
- [ ] `NetworkStreamer.cpp` aparece en el navegador de Xcode
- [ ] `NetworkStreamer.cpp` está en "Compile Sources"
- [ ] Build limpio ejecutado
- [ ] Compilación exitosa sin undefined symbols

---

## 🎯 Estado Actual

- ✅ Font warning: **Corregido**
- ⚠️ Undefined symbols: **Requiere regenerar proyecto**

---

**Última actualización**: 2025-12-06

