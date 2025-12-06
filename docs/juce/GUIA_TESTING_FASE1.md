# Guía de Testing - Fase 1: AudioBufferManager

**Fecha**: 2025-12-06  
**Objetivo**: Verificar que el audio se capture correctamente y se escriba al buffer

---

## 🎯 Recomendación: Usar Standalone Primero

**Ventajas de Standalone para testing inicial:**
- ✅ Más simple: no requiere configurar DAW
- ✅ Debugging más directo: logs en consola
- ✅ Configuración de audio más fácil
- ✅ Permite verificar la UI completa

**Después de verificar en Standalone:**
- Probar en Reaper para validar integración como plugin VST3/AU

---

## 📋 Checklist de Testing

### 1. Preparación

- [ ] Compilar el proyecto (Standalone Plugin)
- [ ] Ejecutar la aplicación Standalone
- [ ] Verificar que la ventana se abre correctamente

### 2. Verificación Inicial (Sin Audio)

**Estado esperado:**
- UI muestra: "Buffer no inicializado (Reproduce audio para activar)"
- Servidor HTTP: Inactivo

**Verificar:**
- [ ] La UI se renderiza correctamente
- [ ] No hay crashes al abrir la aplicación

### 3. Configuración de Audio

**Pasos:**
1. En la aplicación Standalone:
   - Ir a **Audio Settings** (menú o botón)
   - Seleccionar dispositivo de entrada (micrófono o línea de entrada)
   - Seleccionar dispositivo de salida (altavoces/auriculares)
   - Configurar sample rate (44100 Hz recomendado)
   - Configurar buffer size (512 o 1024 samples)

2. **Verificar:**
   - [ ] El audio se configura sin errores
   - [ ] No hay mensajes de error en consola relacionados con audio

### 4. Captura de Audio

**Pasos:**
1. Reproducir audio en el dispositivo de entrada:
   - Hablar en el micrófono, o
   - Reproducir música desde otra aplicación

2. **Observar en la UI:**
   - [ ] El buffer se inicializa automáticamente
   - [ ] "Samples disponibles" aumenta
   - [ ] "Uso del buffer" muestra porcentaje > 0%
   - [ ] "Total samples procesados" incrementa
   - [ ] El indicador de progreso (barra) se llena

3. **Verificar valores:**
   - [ ] Sample Rate: 44100 Hz (o el configurado)
   - [ ] Canales: 1 (mono) o 2 (stereo) según entrada
   - [ ] Tamaño buffer: ~44100 samples (~1 segundo a 44.1kHz)

### 5. Estado del Buffer

**Verificar diferentes estados:**

- [ ] **Buffer vacío** (< 20%): Estado muestra "Vacio"
- [ ] **Buffer normal** (20-80%): Estado muestra "Normal"
- [ ] **Buffer lleno** (> 80%): Estado muestra "Lleno" (color rojo)

### 6. Verificación de Logs

**En la consola de Xcode o Terminal, verificar:**
- [ ] No hay errores de `AudioBufferManager`
- [ ] No hay errores de `NetworkStreamer` (aunque aún no esté activo)
- [ ] Los warnings de HAL son normales (ver sección abajo)

---

## ⚠️ Sobre los Warnings de HAL

Los mensajes que ves en consola son **warnings comunes de macOS Core Audio**:

```
HALC_ProxyIOContext.cpp:1623  HALC_ProxyIOContext::IOWorkLoop: skipping cycle due to overload
HALC_ProxyIOContext.cpp:1631  HALC_ProxyIOContext::IOWorkLoop: context received an out of order message
```

**¿Qué significan?**
- El sistema de audio de macOS está reportando que hay sobrecarga o mensajes fuera de orden
- **Generalmente NO impiden el funcionamiento** del plugin
- Son más comunes cuando:
  - El buffer size es muy pequeño
  - Hay múltiples aplicaciones usando audio simultáneamente
  - El sistema está bajo carga

**¿Cuándo preocuparse?**
- Si el audio se corta o hay glitches
- Si la aplicación crashea
- Si el buffer no se llena aunque haya audio

**Soluciones:**
- Aumentar el buffer size en Audio Settings
- Cerrar otras aplicaciones que usen audio
- Reiniciar la aplicación

---

## 🐛 Troubleshooting

### Problema: Buffer no se inicializa

**Posibles causas:**
- No hay entrada de audio configurada
- El dispositivo de entrada no está activo
- Permisos de micrófono no otorgados (macOS)

**Soluciones:**
- Verificar que el micrófono/entrada esté seleccionada en Audio Settings
- Verificar permisos en System Preferences → Security & Privacy → Microphone
- Probar con otro dispositivo de entrada

### Problema: Buffer se llena muy rápido o muy lento

**Posibles causas:**
- El buffer size es muy pequeño o muy grande
- Hay desincronización entre lectura y escritura

**Soluciones:**
- Ajustar buffer size en Audio Settings
- Verificar que `readFromBuffer` se llame periódicamente (Fase 3)

### Problema: Audio se corta o hay glitches

**Posibles causas:**
- Buffer size muy pequeño
- CPU sobrecargada
- Warnings de HAL indican sobrecarga real

**Soluciones:**
- Aumentar buffer size
- Cerrar otras aplicaciones
- Verificar que no haya procesos pesados corriendo

---

## ✅ Criterios de Éxito

El paso 1 se considera **exitoso** cuando:

1. ✅ La aplicación Standalone se abre sin crashes
2. ✅ El buffer se inicializa cuando hay audio de entrada
3. ✅ La UI muestra valores actualizados en tiempo real:
   - Samples disponibles incrementa
   - Uso del buffer varía según entrada
   - Total samples procesados incrementa
4. ✅ El estado del buffer cambia correctamente (Vacio/Normal/Lleno)
5. ✅ No hay errores críticos en consola (warnings de HAL son aceptables)

---

## 📝 Notas para Testing en Reaper

Una vez verificado en Standalone:

1. **Compilar como VST3 o AU**:
   - En Projucer, asegurar que VST3 o AU esté habilitado
   - Regenerar proyecto y compilar

2. **Cargar en Reaper**:
   - Agregar plugin a un track
   - Configurar entrada de audio en Reaper
   - Verificar que la UI del plugin muestre el mismo comportamiento

3. **Ventajas de Reaper**:
   - Permite grabar el audio procesado
   - Permite usar múltiples pistas
   - Mejor para testing de sincronización (Fase 2)

---

**Última actualización**: 2025-12-06

