/**
 * Phase 2 Investigation: The AEC Channel Hunter (Pulse Test)
 * * Goal: Confirm Ch1/Ch3 are Reference by toggling the tone.
 * * Hardware: Waveshare ESP32-P4 7" (Touch-LCD-7B)
 */

#include <Arduino.h>
#include "AudioManager.h"
#include "BSP_WS_P4_7B.h" 

AudioManager audio;

// Buffer: 20ms of 4-Channel Audio @ 16kHz
#define FRAMES_PER_READ 320 
#define CHANNELS 4
int16_t rawBuffer[FRAMES_PER_READ * CHANNELS];

// --- Tone Globals ---
int16_t *toneCycleBuffer = NULL;
size_t toneCycleSamples = 0;
bool toneEnabled = true;
unsigned long lastToggle = 0;

void drawBar(const char* label, int value, int maxVal) {
    Serial.print(label); Serial.print(": ");
    if (value < 0) value = 0;
    int bars = map(constrain(value, 0, 10000), 0, 10000, 0, 30);
    Serial.print("[");
    for (int i=0; i<30; i++) {
        if (i < bars) Serial.print((i < 10) ? "░" : (i < 20) ? "▒" : "█");
        else Serial.print(" ");
    }
    Serial.print("] ");
    Serial.println(value);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== Audio AEC Probe (Pulse Test) ===");
    
    // Force Power Amp On
    pinMode(53, OUTPUT); digitalWrite(53, HIGH); 

    if (!audio.begin()) {
        Serial.println("CRITICAL: Audio Init Failed!");
        while(1) { delay(100); }
    }

    audio.setVolume(40); 
    
    // Pre-calculate Tone
    uint32_t sampleRate = 16000;
    uint32_t freq = 220;
    size_t waveLength = sampleRate / freq;
    toneCycleSamples = waveLength * 2; 
    
    toneCycleBuffer = (int16_t*)malloc(toneCycleSamples * sizeof(int16_t));
    if (toneCycleBuffer) {
        for (size_t i = 0; i < waveLength; i++) {
            int16_t sample = (i < waveLength / 2) ? 3000 : -3000;
            toneCycleBuffer[i * 2] = sample;     
            toneCycleBuffer[i * 2 + 1] = sample; 
        }
    }
    Serial.println("Setup Complete. Toggling Tone every 3 seconds...");
}

void loop() {
    // 1. Toggle Tone Logic
    if (millis() - lastToggle > 3000) {
        toneEnabled = !toneEnabled;
        lastToggle = millis();
        Serial.printf("\n--- TONE IS NOW: %s ---\n", toneEnabled ? "ON" : "OFF");
    }

    // 2. Play Tone (if enabled)
    if (toneEnabled && toneCycleBuffer) {
        // Fill output buffer to keep pipeline running
        for (int i = 0; i < 70; i++) {
            audio.writeRaw(toneCycleBuffer, toneCycleSamples);
        }
    } else {
        // If Tone OFF, write silence to keep I2S clock running!
        // IMPORTANT: We must keep writing 0s or the DMA stops and Read blocks.
        static int16_t silence[1024] = {0};
        for (int i = 0; i < 10; i++) {
             audio.writeRaw(silence, 512); // Write 512 stereo samples of 0
        }
    }

    // 3. Read Raw TDM Data
    size_t samplesRead = audio.readRaw(rawBuffer, FRAMES_PER_READ * CHANNELS);
    size_t framesRead = samplesRead / CHANNELS;

    if (framesRead > 0) {
        long sumSq[4] = {0, 0, 0, 0};
        for (int i = 0; i < framesRead; i++) {
            for (int ch = 0; ch < 4; ch++) {
                int16_t sample = rawBuffer[i * CHANNELS + ch];
                sumSq[ch] += sample * sample;
            }
        }

        int rms[4];
        for (int ch = 0; ch < 4; ch++) {
            rms[ch] = sqrt(sumSq[ch] / framesRead);
        }

        Serial.println("--- Levels ---");
        drawBar("Slot 0 (Mic 1?)", rms[0], 5000);
        drawBar("Slot 1 (Ref 1?)", rms[1], 5000);
        drawBar("Slot 2 (Mic 2?)", rms[2], 5000);
        drawBar("Slot 3 (Ref 2?)", rms[3], 5000);
        Serial.println("");
    } else {
        delay(10);
    }
}