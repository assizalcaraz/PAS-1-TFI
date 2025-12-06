# Solución: Undefined Symbols de NetworkStreamer

**Fecha**: 2025-12-06

---

## ❌ Error

```
Undefined symbol: NetworkStreamer::stopServer()
Undefined symbol: NetworkStreamer::startServer()
Undefined symbol: NetworkStreamer::NetworkStreamer(AudioBufferManager*, int)
Undefined symbol: NetworkStreamer::getNumClients() const
```

---

## 🔍 Causa

Los símbolos no están definidos porque:
1. El proyecto Xcode no se regeneró después de agregar `NetworkStreamer.cpp` al `.jucer`
2. O el archivo no se está compilando en el target correcto

---

## ✅ Solución

### Paso 1: Regenerar Proyecto Xcode

1. **Abrir Projucer**:
   ```bash
   # Abrir streamAula.jucer en Projucer
   open streamAula.jucer
   ```

2. **Verificar archivos**:
   - En Projucer, verificar que `NetworkStreamer.cpp` y `NetworkStreamer.h` aparecen en la lista de archivos
   - Ambos deben estar marcados para compilar

3. **Guardar proyecto**:
   - File → Save Project
   - Esto regenerará el proyecto Xcode con todos los archivos

### Paso 2: Limpiar Build en Xcode

1. **Abrir proyecto Xcode**:
   ```bash
   open streamAula/Builds/MacOSX/streamAula.xcodeproj
   ```

2. **Limpiar build**:
   - Product → Clean Build Folder (⇧⌘K)

3. **Verificar archivos en Xcode**:
   - En el navegador de proyectos, verificar que `NetworkStreamer.cpp` aparece
   - Debe estar en el target "streamAula - Shared Code"

### Paso 3: Recompilar

1. **Build**:
   - Product → Build (⌘B)
   - Verificar que no hay errores de linking

---

## 🔧 Verificación

Si después de regenerar el proyecto aún hay errores:

### Verificar que NetworkStreamer.cpp está en el target

1. En Xcode, seleccionar `NetworkStreamer.cpp`
2. Abrir "File Inspector" (⌥⌘1)
3. Verificar que está marcado para el target correcto

### Verificar compilación

1. Verificar que `NetworkStreamer.cpp` aparece en "Compile Sources"
2. Build Phases → Compile Sources → Debe incluir `NetworkStreamer.cpp`

---

## 📝 Nota sobre Font Warning

El warning de `Font` deprecated ya está corregido usando `FontOptions` directamente.

---

**Última actualización**: 2025-12-06

