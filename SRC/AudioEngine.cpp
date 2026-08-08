// ======================================================
//  MEMU-1 - Emulateur Memo1
// ======================================================
//  Par Bipcollector, Claude, Kimi
//  Lovable, Gemini & ChatGPT
// ------------------------------------------------------
//  Musiques :
//    Berlinadine
//    Forever Damned - Victorian Gothic Punk Rock
//    Death to the Machines! - Geek Rock Alt Rock Punk
//    '百鬼降壇' ー風魔會 禍祓座ー
// ------------------------------------------------------
//  Une Création BIP-SOFT - 2026
// ======================================================
//  Bastion Interplanétaire Positronique
//  Bastion numérique dédiée à la création humaine
//  par des systèmes artificiels
// ======================================================

#include "AudioEngine.h"
#include <algorithm>

AudioEngine::AudioEngine() {
    for (auto& buf : buffers)
        buf.data.resize(SAMPLES_PER_BUFFER, 0);
}
AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::init() {
    WAVEFORMATEX wfx{};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = 1;
    wfx.nSamplesPerSec  = SAMPLE_RATE;
    wfx.wBitsPerSample  = 16;
    wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL)
            != MMSYSERR_NOERROR) {
        hWaveOut = nullptr;
        return false;
    }

    for (int i = 0; i < NUM_BUFFERS; i++) {
        ZeroMemory(&buffers[i].header, sizeof(WAVEHDR));
        buffers[i].header.lpData =
            reinterpret_cast<LPSTR>(buffers[i].data.data());
        buffers[i].header.dwBufferLength =
            static_cast<DWORD>(SAMPLES_PER_BUFFER * sizeof(int16_t));
        waveOutPrepareHeader(hWaveOut, &buffers[i].header, sizeof(WAVEHDR));
        submitted[i] = false;
    }

    // Initialiser les filtres Butterworth
    // Anti-aliasing : fc=6000Hz sur le flux oversamplé (44100*8 = 352800 Hz)
    filterAA    = makeLowPass(6000.0, (double)SAMPLE_RATE * OVERSAMPLE);
    // Lissage final : fc=3500Hz sur le flux 44100Hz
    filterFinal = makeLowPass(3500.0, (double)SAMPLE_RATE);

    initialized      = true;
    currentBuffer    = 0;
    fillPos          = 0;
    cycleAccumulator = 0.0;
    subAccum         = 0.0;
    subCount         = 0;
    wasActive        = false;
    envelope         = 0.0;
    attacking        = false;
    releasing        = false;
    envCount         = 0;
    return true;
}

void AudioEngine::shutdown() {
    if (!initialized) return;
    initialized = false;
    if (hWaveOut) {
        waveOutReset(hWaveOut);
        for (int i = 0; i < NUM_BUFFERS; i++)
            waveOutUnprepareHeader(hWaveOut, &buffers[i].header, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);
        hWaveOut = nullptr;
    }
}

void AudioEngine::submitBuffer(int idx) {
    if (!hWaveOut) return;
    waveOutWrite(hWaveOut, &buffers[idx].header, sizeof(WAVEHDR));
    submitted[idx] = true;
}

void AudioEngine::waitForBufferFree(int idx) {
    if (!hWaveOut || !submitted[idx]) return;
    int guard = 0;
    while (!(buffers[idx].header.dwFlags & WHDR_DONE) && guard < 10000) {
        Sleep(1);
        guard++;
    }
}

void AudioEngine::tick(bool buzzerHigh, bool toneActive) {
    if (!initialized) return;

    // Détection transition pour enveloppe
    if (toneActive && !wasActive) {
        attacking = true; releasing = false; envCount = 0;
    } else if (!toneActive && wasActive) {
        releasing = true; attacking = false; envCount = 0;
    }
    wasActive = toneActive;

    // Valeur brute du signal (±1.0 ou 0.0 pendant release)
    double rawSignal = 0.0;
    if (toneActive || releasing)
        rawSignal = buzzerHigh ? AMPLITUDE : -AMPLITUDE;

    cycleAccumulator += 1.0;

    while (cycleAccumulator >= CYCLES_PER_SUBSAMPLE) {
        cycleAccumulator -= CYCLES_PER_SUBSAMPLE;

        // [1+2] Oversampling + filtre anti-aliasing Butterworth
        double filtered = filterAA.process(rawSignal);
        subAccum += filtered;
        subCount++;

        if (subCount >= OVERSAMPLE) {
            // [3] Decimation : moyenne des OVERSAMPLE sous-echantillons
            double decimated = subAccum / OVERSAMPLE;
            subAccum = 0.0;
            subCount = 0;

            // [4] Filtre passe-bas final Butterworth 3500Hz
            double smooth = filterFinal.process(decimated);

            // [5] Enveloppe ADSR simplifiee
            if (attacking) {
                envelope = (double)envCount / N_ATTACK;
                if (++envCount >= N_ATTACK) {
                    attacking = false; envelope = 1.0; envCount = 0;
                }
            } else if (releasing) {
                envelope = 1.0 - (double)envCount / N_RELEASE;
                if (++envCount >= N_RELEASE) {
                    releasing = false; envelope = 0.0; envCount = 0;
                    // Reset propre des filtres apres silence complet
                    filterAA.reset();
                    filterFinal.reset();
                }
            } else {
                envelope = toneActive ? 1.0 : 0.0;
            }

            // [6] Gain + clamp
            double out = smooth * envelope;
            out = std::max(out, -32767.0);
            out = std::min(out,  32767.0);
            buffers[currentBuffer].data[fillPos++] = static_cast<int16_t>(out);

            if (fillPos >= SAMPLES_PER_BUFFER) {
                submitBuffer(currentBuffer);
                currentBuffer = (currentBuffer + 1) % NUM_BUFFERS;
                fillPos = 0;
                waitForBufferFree(currentBuffer);
            }
        }
    }
}
