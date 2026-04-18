// Obtener configuración del servidor
let configAudio = { sampleRate: 44100, channels: 1, formato: 'pcm' };

/** Opciones de fetch: evita respuestas cacheadas y no reutiliza conexiones de forma opaca entre pestañas. */
const fetchNoCache = { cache: 'no-store' };

async function obtenerConfig() {
    try {
        const response = await fetch('/config', fetchNoCache);
        const config = await response.json();
        // El servidor envía "sampleRate" (camelCase), no "sample_rate"
        configAudio.sampleRate = config.sampleRate || config.sample_rate || 44100;
        configAudio.channels = config.channels || 1;
        configAudio.formato = config.formato || 'pcm';
        console.log('✅ Configuración obtenida del servidor:', config);
        console.log('✅ Configuración de audio:', configAudio);
    } catch (error) {
        console.warn('⚠️ No se pudo obtener configuración, usando valores por defecto', error);
    }
}

// Usar Web Audio API para reproducir audio
let audioContext = null;
const status = document.getElementById('status');

let isPlaying = false;
let bufferAcumulado = new Uint8Array(0);
/** La API Fetch suele decodificar ya el chunked encoding; el body es PCM int16 continuo (no re-parsear hex). */

// Enfoque simple: acumular datos y reproducir cuando tengamos suficiente
let audioBufferAcumulado = null; // Buffer de audio acumulado para reproducción continua
let muestrasAcumuladas = 0;
const muestrasPorBuffer = 8192; // Buffer más grande para reproducción más fluida (~170ms a 48kHz)

// Variables para resampling si es necesario
let necesitaResampling = false;
let ratioResampling = 1.0;
let bufferResampling = null; // Buffer para acumular muestras durante resampling

function actualizarEstado(mensaje, color) {
    status.textContent = mensaje;
    status.style.background = color;
    console.log(mensaje);
}

function mergeUint8(a, b) {
    if (b.length === 0)
        return a;
    const out = new Uint8Array(a.length + b.length);
    out.set(a, 0);
    out.set(b, a.length);
    return out;
}

function yieldToMain() {
    return new Promise((resolve) => setTimeout(resolve, 0));
}

async function iniciarStreaming() {
    try {
        await obtenerConfig();
        
        actualizarEstado('Inicializando...', '#333');
        
        // Crear contexto de audio
        // NOTA: Algunos navegadores ignoran el sampleRate en el constructor
        // Por eso verificamos el sampleRate real después de crear el contexto
        audioContext = new (window.AudioContext || window.webkitAudioContext)({
            sampleRate: configAudio.sampleRate
        });
        
        // Verificar el sampleRate real del AudioContext
        const sampleRateReal = audioContext.sampleRate;
        const sampleRateServidor = configAudio.sampleRate;
        
        // Logs muy visibles para diagnóstico
        console.log('========================================');
        console.log('🎵 DIAGNÓSTICO DE SAMPLERATE:');
        console.log('🎵 SampleRate solicitado al navegador:', sampleRateServidor, 'Hz');
        console.log('🎵 SampleRate REAL del AudioContext:', sampleRateReal, 'Hz');
        console.log('🎵 Diferencia:', Math.abs(sampleRateReal - sampleRateServidor), 'Hz');
        console.log('========================================');
        
        // CRÍTICO: Si hay discrepancia, el navegador hará resampling automático
        // que causará que el audio suene más grave o más agudo
        if (Math.abs(sampleRateReal - sampleRateServidor) > 1) {
            console.error('========================================');
            console.error('❌ ERROR CRÍTICO: Discrepancia de sampleRate detectada!');
            console.error('❌ Servidor envía a:', sampleRateServidor, 'Hz');
            console.error('❌ Navegador reproduce a:', sampleRateReal, 'Hz');
            console.error('❌ El navegador hará resampling automático, causando distorsión de pitch');
            console.error('❌ Si suena más GRAVE: servidor > navegador (ej: 48kHz -> 44.1kHz)');
            console.error('❌ Si suena más AGUDO: servidor < navegador (ej: 44.1kHz -> 48kHz)');
            console.error('========================================');
            actualizarEstado('ERROR: SampleRate incompatible (' + sampleRateServidor + ' vs ' + sampleRateReal + ' Hz)', '#a00');
            
            // NO ajustar configAudio - mantener el del servidor para hacer resampling manual
            // Si el navegador no soporta el sampleRate del servidor, debemos hacer resampling
            necesitaResampling = true;
            ratioResampling = sampleRateServidor / sampleRateReal;
            console.warn('⚠️ ACTIVANDO RESAMPLING MANUAL');
            console.warn('⚠️ Ratio de resampling:', ratioResampling.toFixed(4));
            console.warn('⚠️ Esto convertirá el audio del servidor al sampleRate del navegador');
        } else {
            necesitaResampling = false;
            ratioResampling = 1.0;
            console.log('✅ SampleRate compatible - no se necesita resampling');
            console.log('✅ El audio debería sonar con el pitch correcto');
        }
        
        // Activar contexto de audio
        if (audioContext.state === 'suspended') {
            await audioContext.resume();
        }
        
        actualizarEstado('Conectando al servidor...', '#333');
        
        console.log('🔗 Iniciando conexión al stream...');
        const response = await fetch('/stream', fetchNoCache);
        if (!response.ok) {
            throw new Error('Error de conexión: ' + response.status);
        }
        
        console.log('✅ Conexión establecida, Content-Type:', response.headers.get('Content-Type'));
        actualizarEstado('Conectado, recibiendo audio...', '#333');
        
        // Verificar que el body esté disponible
        if (!response.body) {
            throw new Error('Response body no disponible');
        }
        
        const reader = response.body.getReader();
        isPlaying = true;
        
        // Inicializar buffers y contadores
        audioBufferAcumulado = null;
        muestrasAcumuladas = 0;
        bufferAcumulado = new Uint8Array(0);
        chunksReproducidos = 0;
        
        console.log('📖 Iniciando lectura del stream...');
        let primerChunkRecibido = false;
        
        while (isPlaying) {
            let readResult;
            try {
                readResult = await Promise.race([
                    reader.read(),
                    new Promise((_, reject) => 
                        setTimeout(() => reject(new Error('Timeout leyendo stream')), 5000)
                    )
                ]);
            } catch (error) {
                console.error('Error al leer stream:', error);
                actualizarEstado('Error: ' + error.message, '#a00');
                break;
            }
            
            const { done, value } = readResult;
            
            if (done) {
                console.log('Stream finalizado');
                actualizarEstado('Conexión cerrada', '#a00');
                isPlaying = false; // Detener reproducción
                break;
            }
            
            if (!primerChunkRecibido && value && value.length > 0) {
                primerChunkRecibido = true;
                console.log('✅ Primer bloque recibido del servidor:', value.length, 'bytes (PCM continuo vía Fetch)');
            }

            if (value != null && value.byteLength > 0)
                bufferAcumulado = mergeUint8(bufferAcumulado, value);

            // Procesar según el formato
            if (configAudio.formato === 'mp3') {
                // Para MP3, usar procesamiento especial
                await procesarMP3Chunk(bufferAcumulado);
                bufferAcumulado = new Uint8Array(0);
            } else {
                // PCM raw
                // Calcular bytes necesarios según canales
                const bytesPorMuestra = 2; // int16 = 2 bytes
                const muestrasNecesarias = 1024;
                const bytesNecesarios = muestrasNecesarias * bytesPorMuestra * configAudio.channels;
                
                // Procesar frames PCM; ceder el hilo cada varios frames para no congelar la pestaña
                let framesProcesados = 0;
                while (bufferAcumulado.length >= bytesNecesarios) {
                    const chunkBytes = bufferAcumulado.slice(0, bytesNecesarios);
                    bufferAcumulado = bufferAcumulado.slice(bytesNecesarios);
                    
                    // Crear Int16Array desde el buffer (little-endian)
                    const totalMuestras = chunkBytes.byteLength / 2;
                    const muestrasPorCanal = totalMuestras / configAudio.channels;
                    const int16Array = new Int16Array(
                        chunkBytes.buffer, 
                        chunkBytes.byteOffset, 
                        totalMuestras
                    );
                    
                    // Convertir a float32 (-1.0 a 1.0)
                    // Si es estéreo, mezclar a mono o usar ambos canales
                    const float32Array = new Float32Array(muestrasPorCanal);
                    let maxAbsValue = 0;
                    
                    // Conversión correcta de int16 a float32
                    // int16 range: -32768 a 32767
                    // float32 range: -1.0 a 1.0
                    // Normalización: dividir por 32768.0 (no 32767) para mantener simetría
                    if (configAudio.channels === 1) {
                        // Mono: usar directamente
                        for (let i = 0; i < muestrasPorCanal; i++) {
                            const int16Value = int16Array[i];
                            // Convertir int16 a float32 normalizado
                            float32Array[i] = int16Value / 32768.0;
                            maxAbsValue = Math.max(maxAbsValue, Math.abs(float32Array[i]));
                        }
                    } else {
                        // Estéreo: mezclar canales (promedio)
                        for (let i = 0; i < muestrasPorCanal; i++) {
                            const left = int16Array[i * 2] / 32768.0;
                            const right = int16Array[i * 2 + 1] / 32768.0;
                            const mixed = (left + right) / 2.0;
                            float32Array[i] = mixed;
                            maxAbsValue = Math.max(maxAbsValue, Math.abs(float32Array[i]));
                        }
                    }
                    
                    // Verificar que hay datos (no todo ceros)
                    if (maxAbsValue > 0.001) {
                        if (chunksReproducidos === 0) {
                            console.log('🔊 Primer chunk con audio detectado (max:', maxAbsValue.toFixed(4), ', canales:', configAudio.channels, ')');
                        }
                    } else if (chunksReproducidos === 0) {
                        console.warn('⚠️ Primer chunk recibido pero parece ser silencio (canales:', configAudio.channels, ')');
                    }
                    
                    // Log del primer chunk procesado para verificar configuración
                    if (chunksReproducidos === 0) {
                        console.log('🎵 Primer chunk procesado:');
                        console.log('  - Muestras por canal:', muestrasPorCanal);
                        console.log('  - Canales:', configAudio.channels);
                        console.log('  - SampleRate configurado:', configAudio.sampleRate, 'Hz');
                        console.log('  - SampleRate del AudioContext:', audioContext.sampleRate, 'Hz');
                        console.log('  - Max abs value:', maxAbsValue.toFixed(6));
                    }
                    
                    // Enfoque simple: acumular muestras y reproducir cuando tengamos suficiente
                    acumularYReproducir(float32Array);

                    if ((++framesProcesados & 31) === 0)
                        await yieldToMain();
                }
                
                // Log si el buffer está creciendo demasiado (posible problema de procesamiento)
                if (bufferAcumulado.length > bytesNecesarios * 10) {
                    console.warn('⚠️ Buffer acumulado muy grande:', bufferAcumulado.length, 'bytes (esperado:', bytesNecesarios, ')');
                    // Descartar datos antiguos para evitar delay excesivo
                    const descartar = bufferAcumulado.length - bytesNecesarios * 2;
                    if (descartar > 0) {
                        bufferAcumulado = bufferAcumulado.slice(descartar);
                        console.log('🗑️ Descartados', descartar, 'bytes antiguos para reducir delay');
                    }
                }
            }
        }
    } catch (error) {
        console.error('Error en streaming:', error);
        actualizarEstado('Error: ' + error.message, '#a00');
        isPlaying = false;
    } finally {
        // Limpiar buffers al detener
        audioBufferAcumulado = null;
        muestrasAcumuladas = 0;
        bufferAcumulado = new Uint8Array(0);
        chunksReproducidos = 0;
        nextPlayTime = 0;
        necesitaResampling = false;
        ratioResampling = 1.0;
        bufferResampling = null;
    }
}

async function procesarMP3Chunk(chunk) {
    // Para MP3, usar el elemento <audio> que soporta MP3 nativamente
    // Acumular chunks MP3 y reproducir cuando tengamos suficiente
    bufferAcumulado = new Uint8Array(bufferAcumulado.length + chunk.length);
    bufferAcumulado.set(bufferAcumulado.slice(0, bufferAcumulado.length - chunk.length));
    bufferAcumulado.set(chunk, bufferAcumulado.length - chunk.length);
    
    // Si tenemos suficiente datos, crear un blob y reproducir
    if (bufferAcumulado.length > 8192) { // ~8KB mínimo
        const blob = new Blob([bufferAcumulado], { type: 'audio/mpeg' });
        const url = URL.createObjectURL(blob);
        
        const audio = new Audio(url);
        audio.play().catch(e => console.error('Error al reproducir MP3:', e));
        
        bufferAcumulado = new Uint8Array(0);
    }
}

let chunksReproducidos = 0;
let nextPlayTime = 0;

// Resampling simple usando interpolación lineal
function resampleAudio(inputArray, inputSampleRate, outputSampleRate) {
    if (inputSampleRate === outputSampleRate) {
        return inputArray; // No se necesita resampling
    }
    
    const ratio = inputSampleRate / outputSampleRate;
    const outputLength = Math.round(inputArray.length / ratio);
    const outputArray = new Float32Array(outputLength);
    
    // Log solo la primera vez para no saturar la consola
    if (!resampleAudio._logged) {
        console.log('🔄 Aplicando resampling:', inputSampleRate, 'Hz ->', outputSampleRate, 'Hz');
        console.log('🔄 Ratio:', ratio.toFixed(4), ', Muestras:', inputArray.length, '->', outputLength);
        resampleAudio._logged = true;
    }
    
    for (let i = 0; i < outputLength; i++) {
        const srcIndex = i * ratio;
        const srcIndexFloor = Math.floor(srcIndex);
        const srcIndexCeil = Math.min(srcIndexFloor + 1, inputArray.length - 1);
        const fraction = srcIndex - srcIndexFloor;
        
        // Interpolación lineal
        outputArray[i] = inputArray[srcIndexFloor] * (1 - fraction) + inputArray[srcIndexCeil] * fraction;
    }
    
    return outputArray;
}

// Enfoque simple: acumular muestras y reproducir cuando tengamos un buffer completo
function acumularYReproducir(float32Array) {
    try {
        // Si necesitamos resampling, hacerlo antes de acumular
        let muestrasParaAcumular = float32Array;
        if (necesitaResampling) {
            // Resample de sampleRate del servidor al sampleRate del AudioContext
            // CRÍTICO: Esto corrige el problema de que el audio suene más grave o más agudo
            muestrasParaAcumular = resampleAudio(
                float32Array,
                configAudio.sampleRate,  // SampleRate del servidor
                audioContext.sampleRate // SampleRate del AudioContext
            );
            
            // Log solo la primera vez para verificar que se está aplicando
            if (!acumularYReproducir._resampleLogged) {
                console.log('🔄 RESAMPLING ACTIVO:', configAudio.sampleRate, 'Hz ->', audioContext.sampleRate, 'Hz');
                console.log('🔄 Muestras antes:', float32Array.length, ', después:', muestrasParaAcumular.length);
                acumularYReproducir._resampleLogged = true;
            }
        } else {
            // Verificar que realmente no se necesita resampling
            if (!acumularYReproducir._noResampleLogged) {
                console.log('✅ No se necesita resampling - sampleRates compatibles');
                acumularYReproducir._noResampleLogged = true;
            }
        }
        
        // Inicializar buffer acumulado si es necesario
        if (audioBufferAcumulado === null) {
            audioBufferAcumulado = new Float32Array(muestrasPorBuffer);
            muestrasAcumuladas = 0;
            // Inicializar nextPlayTime cuando empezamos
            nextPlayTime = audioContext.currentTime + 0.1; // 100ms de buffer inicial
        }
        
        // Copiar muestras al buffer acumulado
        let muestrasRestantes = muestrasParaAcumular.length;
        let offsetOrigen = 0;
        
        while (muestrasRestantes > 0) {
            const espacioDisponible = muestrasPorBuffer - muestrasAcumuladas;
            const muestrasACopiar = Math.min(muestrasRestantes, espacioDisponible);
            
            audioBufferAcumulado.set(
                muestrasParaAcumular.subarray(offsetOrigen, offsetOrigen + muestrasACopiar),
                muestrasAcumuladas
            );
            
            muestrasAcumuladas += muestrasACopiar;
            offsetOrigen += muestrasACopiar;
            muestrasRestantes -= muestrasACopiar;
            
            // Si el buffer está lleno, reproducirlo
            if (muestrasAcumuladas >= muestrasPorBuffer) {
                reproducirBufferSimple(audioBufferAcumulado);
                // Reiniciar buffer
                audioBufferAcumulado = new Float32Array(muestrasPorBuffer);
                muestrasAcumuladas = 0;
            }
        }
    } catch (e) {
        console.error('Error al acumular y reproducir:', e);
    }
}

// Reproducir un buffer de forma simple y continua
function reproducirBufferSimple(float32Array) {
    try {
        const ahora = audioContext.currentTime;
        
        // CRÍTICO: Siempre usar el sampleRate REAL del AudioContext
        // Si el servidor envía a 48kHz pero el navegador reproduce a 44.1kHz,
        // el resampling ya se hizo en acumularYReproducir, así que float32Array
        // ya está en el sampleRate correcto del AudioContext
        const sampleRateParaBuffer = audioContext.sampleRate;
        
        // Crear AudioBuffer con el sampleRate del AudioContext (no del servidor)
        // Esto evita resampling automático del navegador que causa distorsión
        const audioBuffer = audioContext.createBuffer(1, float32Array.length, sampleRateParaBuffer);
        audioBuffer.copyToChannel(float32Array, 0);
        
        // Asegurar que nextPlayTime no esté en el pasado
        if (nextPlayTime < ahora) {
            nextPlayTime = ahora + 0.01; // Pequeño buffer para evitar gaps
        }
        
        // Si el delay es excesivo (>300ms), resetear
        if (nextPlayTime > ahora + 0.3) {
            nextPlayTime = ahora + 0.05; // Resetear con buffer pequeño
        }
        
        const source = audioContext.createBufferSource();
        source.buffer = audioBuffer;
        source.connect(audioContext.destination);
        
        // Programar reproducción
        source.start(nextPlayTime);
        
        // Actualizar nextPlayTime para continuidad
        nextPlayTime += audioBuffer.duration;
        
        chunksReproducidos++;
        if (chunksReproducidos === 1) {
            console.log('✅ Primer buffer reproducido');
            actualizarEstado('Reproduciendo audio', '#0a0');
        }
    } catch (e) {
        console.error('Error al reproducir buffer:', e);
    }
}


// Botón para iniciar streaming
const btnIniciar = document.getElementById('btnIniciar');

async function iniciarStreamingConUI() {
    if (!isPlaying) {
        btnIniciar.disabled = true;
        btnIniciar.textContent = 'Conectando...';
        btnIniciar.style.background = '#333';
        
        try {
            await iniciarStreaming();
            if (isPlaying) {
                btnIniciar.textContent = '🔊 Reproduciendo...';
                btnIniciar.style.background = '#0a0';
            }
        } catch (error) {
            btnIniciar.disabled = false;
            btnIniciar.textContent = '▶️ Reintentar';
            btnIniciar.style.background = '#a00';
        }
    }
}

btnIniciar.addEventListener('click', iniciarStreamingConUI);

// Detener al cerrar
window.addEventListener('beforeunload', () => {
    isPlaying = false;
});

