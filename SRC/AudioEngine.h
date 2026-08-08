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

#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <windows.h>
#include <mmsystem.h>
#include <cstdint>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  AudioEngine v3 -- Son net et precis pour le Memo-1
// ============================================================
//  Chaine de traitement du signal :
//
//  Signal carre brut (PB7)
//    |
//    v  [1] OVERSAMPLING x8
//         On genere 8 sous-echantillons par sample audio.
//         Permet de capturer precisement le front de l'onde
//         carree meme quand il tombe entre deux echantillons
//         -> supprime l'aliasing (sifflement parasite).
//    |
//    v  [2] FILTRE ANTI-ALIASING (Butterworth 2nd ordre, fc=6kHz)
//         Applique sur les 8 sous-echantillons avant decimation.
//         Pente -40dB/decade, phase lineaire.
//    |
//    v  [3] DECIMATION (8->1)
//         On garde 1 sample sur 8 apres filtrage.
//    |
//    v  [4] FILTRE PASSE-BAS FINAL (Butterworth 2nd ordre, fc=3500Hz)
//         Adoucit l'onde carree : retire les harmoniques agressives
//         tout en preservant les fondamentales musicales (260-500Hz).
//         Rendu similaire a un vrai buzzer piezo charge.
//    |
//    v  [5] ENVELOPPE ADSR simplifiee (Attack 3ms / Release 5ms)
//         Fondu a la montee et a la descente pour zero clic DC.
//    |
//    v  [6] GAIN FINAL + CLAMP int16
// ============================================================

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool init();
    void shutdown();

    void tick(bool buzzerHigh, bool toneActive);

private:
    static constexpr int     SAMPLE_RATE        = 44100;
    static constexpr int     CPU_HZ             = 750000;
    static constexpr int     OVERSAMPLE         = 8;
    static constexpr int     SAMPLES_PER_BUFFER = 1024;
    static constexpr int     NUM_BUFFERS        = 6;
    static constexpr double  AMPLITUDE          = 28000.0; // avant filtre (attenue ~40%)

    // Attack  : 3ms  = 132 samples
    // Release : 5ms  = 220 samples
    static constexpr int N_ATTACK  = 132;
    static constexpr int N_RELEASE = 220;

    // ── Filtre Butterworth 2nd ordre ─────────────────────────────
    // Coefficients calcules pour fc=6000Hz @ (44100*8) Hz (anti-alias)
    // et fc=3500Hz @ 44100Hz (lissage final)
    // Forme : y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2]
    //              - a1*y[n-1] - a2*y[n-2]
    struct Biquad {
        double b0=1, b1=0, b2=0, a1=0, a2=0;
        double x1=0, x2=0, y1=0, y2=0;
        double process(double x) {
            double y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
            x2=x1; x1=x; y2=y1; y1=y;
            return y;
        }
        void reset() { x1=x2=y1=y2=0.0; }
    };

    // Calcule les coefficients d'un filtre passe-bas Butterworth 2nd ordre
    // fc = frequence de coupure, fs = frequence d'echantillonnage
    static Biquad makeLowPass(double fc, double fs) {
        Biquad f;
        double w0    = 2.0 * M_PI * fc / fs;
        double cosw  = std::cos(w0);
        double sinw  = std::sin(w0);
        double alpha = sinw / (2.0 * std::sqrt(2.0)); // Q = 1/sqrt(2) = Butterworth
        double a0    = 1.0 + alpha;
        f.b0 = (1.0 - cosw) / 2.0 / a0;
        f.b1 =  (1.0 - cosw)      / a0;
        f.b2 = (1.0 - cosw) / 2.0 / a0;
        f.a1 = -2.0 * cosw         / a0;
        f.a2 = (1.0 - alpha)       / a0;
        return f;
    }

    struct Buffer {
        std::vector<int16_t> data;
        WAVEHDR              header{};
    };

    HWAVEOUT hWaveOut      = nullptr;
    Buffer   buffers[NUM_BUFFERS];
    bool     submitted[NUM_BUFFERS] = {};
    int      currentBuffer = 0;
    int      fillPos       = 0;
    bool     initialized   = false;

    double cycleAccumulator = 0.0;
    // Cycles CPU par sous-echantillon oversamples
    static constexpr double CYCLES_PER_SUBSAMPLE =
        static_cast<double>(CPU_HZ) /
        (static_cast<double>(SAMPLE_RATE) * OVERSAMPLE);

    // Filtres
    Biquad filterAA;    // anti-aliasing @ 6kHz (rate oversampled)
    Biquad filterFinal; // lissage final @ 3500Hz (rate normal)

    // Enveloppe
    bool   wasActive = false;
    double envelope  = 0.0;   // 0.0 -> 1.0
    bool   attacking = false;
    bool   releasing = false;
    int    envCount  = 0;

    // Oversampling : accumulateur de sous-echantillons
    double subAccum = 0.0;
    int    subCount = 0;

    void submitBuffer(int idx);
    void waitForBufferFree(int idx);
};
