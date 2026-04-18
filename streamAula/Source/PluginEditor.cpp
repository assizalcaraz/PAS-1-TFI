/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// Helper para crear String desde literales UTF-8 de forma segura
// Evita problemas de aserción en juce_String.cpp con caracteres no ASCII
static inline juce::String U8(const char* utf8) 
{ 
    return juce::String(juce::CharPointer_UTF8(utf8)); 
}

//==============================================================================
StreamAulaAudioProcessorEditor::StreamAulaAudioProcessorEditor (StreamAulaAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (500, 400);
    
    // Iniciar timer para actualizar la UI cada 100ms
    startTimer(100);
}

StreamAulaAudioProcessorEditor::~StreamAulaAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void StreamAulaAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Fondo
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // Título
    g.setColour (juce::Colours::white);
    juce::Font titleFont (juce::FontOptions (24.0f));
    titleFont = titleFont.boldened();
    g.setFont (titleFont);
    g.drawFittedText ("streamAula", getLocalBounds().removeFromTop(40), juce::Justification::centred, 1);
    
    // Información del buffer
    auto* bufferManager = audioProcessor.getAudioBufferManager();
    auto bounds = getLocalBounds().withTrimmedTop(50);
    
    g.setFont (juce::FontOptions (14.0f));
    
    if (bufferManager != nullptr && audioProcessor.isBufferActive())
    {
        int availableSamples = bufferManager->getAvailableSamples();
        int freeSpace = bufferManager->getFreeSpace();
        int64_t sampleCount = bufferManager->getCurrentSampleCount();
        int bufferSize = bufferManager->getBufferSize();
        // Tasa real del host (JUCE); coincide con la que recibió prepareToPlay.
        const double sampleRateHost = audioProcessor.getSampleRate();
        const double sampleRateBuf = bufferManager->getSampleRate();
        int numChannels = bufferManager->getNumChannels();

        // Ocupación = muestras listas para el lector (fan-out), no (bufferSize - freeSpace):
        // con FIFO vacío JUCE suele reportar freeSpace == bufferSize - 1 (p. ej. 71999),
        // lo que daba ~0% fijo aunque el stream esté sano.
        const float usagePercent = juce::jlimit (0.0f, 100.0f,
                                                 (float) availableSamples / (float) juce::jmax (1, bufferSize) * 100.0f);
        
        // Estado del buffer
        juce::String statusText;
        statusText << "Estado: ";
        if (usagePercent > 80.0f)
            statusText << "Lleno";
        else if (usagePercent < 20.0f)
            statusText << "Vacio";
        else
            statusText << "Normal";
        
        g.setColour (juce::Colours::lightgrey);
        g.drawFittedText (statusText, bounds.removeFromTop(25), juce::Justification::centredLeft, 1);
        
        // Información detallada
        juce::String infoText;
        infoText << U8("Muestras listas (streaming): ") << availableSamples << "\n";
        infoText << U8("Espacio libre p/escritura (host): ") << freeSpace << "\n";
        infoText << U8("Ocupaci\xc3\xb3n (datos pendientes): ") << juce::String::formatted ("%.1f", usagePercent) << "%\n";
        infoText << "Total samples procesados: " << sampleCount << "\n";
        infoText << U8("Tasa de muestreo (motor del host / DAW): ")
                 << juce::String::formatted ("%.0f", sampleRateHost) << U8(" Hz\n");
        if (std::abs (sampleRateHost - sampleRateBuf) > 0.5)
            infoText << U8("(!) Buffer interno reporta ") << juce::String::formatted ("%.0f", sampleRateBuf)
                     << U8(" Hz (no deber\xc3\xad""a ocurrir; reinicie reproducci\xc3\xb3n)\n");
        infoText << U8("Canales: ") << numChannels << "\n";
        infoText << U8("Tama\xc3\xb1o buffer: ") << bufferSize << U8(" muestras (~")
                 << juce::String::formatted ("%.2f", bufferSize / sampleRateHost) << U8(" s)");
        
        g.setColour (juce::Colours::white);
        g.drawFittedText (infoText, bounds.removeFromTop(200), juce::Justification::centredLeft, 7);
        
        // Barra de progreso del buffer
        auto progressBounds = bounds.removeFromTop(20).reduced(20, 5);
        g.setColour (juce::Colours::darkgrey);
        g.fillRoundedRectangle (progressBounds.toFloat(), 3.0f);
        
        g.setColour (usagePercent > 80.0f ? juce::Colours::red : 
                    usagePercent > 50.0f ? juce::Colours::orange : 
                    juce::Colours::green);
        auto filledBounds = progressBounds.withWidth ((int)(progressBounds.getWidth() * usagePercent / 100.0f));
        g.fillRoundedRectangle (filledBounds.toFloat(), 3.0f);
        
        // Información del servidor
        bounds.removeFromTop(10);
        auto* networkStreamer = audioProcessor.getNetworkStreamer();
        if (networkStreamer != nullptr && audioProcessor.isServerActive())
        {
            int numClients = networkStreamer->getNumClients();
            int port = networkStreamer->getPort();
            
            juce::String serverText;
            serverText << U8("Servidor HTTP: Activo en puerto ") << port << "\n";
            serverText << U8("Clientes conectados: ") << numClients;
            
            g.setColour (juce::Colours::lightgreen);
            g.setFont (juce::FontOptions (12.0f));
            g.drawFittedText (serverText, bounds.removeFromTop(40), juce::Justification::centredLeft, 2);
        }
        else
        {
            g.setColour (juce::Colours::orange);
            g.setFont (juce::FontOptions (12.0f));
            g.drawFittedText (U8("Servidor HTTP: Inactivo"), bounds.removeFromTop(20), juce::Justification::centredLeft, 1);
        }
    }
    else
    {
        g.setColour (juce::Colours::orange);
        g.drawFittedText (U8("Buffer no inicializado\n(Reproduce audio para activar)"), 
                         bounds, juce::Justification::centred, 2);
    }
}

void StreamAulaAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

//==============================================================================
void StreamAulaAudioProcessorEditor::timerCallback()
{
    // Actualizar la UI periódicamente para mostrar información actualizada del buffer
    repaint();
}
