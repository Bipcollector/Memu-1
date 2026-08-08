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
// ============================================================
//  KCS.h  —  Kansas City Standard 300 baud pour le Memo-1
// ============================================================
//  Format d'un bloc KCS Memo-1 :
//    [silence 0.5s] [leader 1500 x bit-1 @ 2400Hz ~5s]
//    [magic 'M' 0x4D, '1' 0x31]
//    [start_addr 2o LE] [length 2o LE]
//    [data N octets]
//    [checksum 1o = XOR de tous les octets data]
//    [silence 0.5s]
//
//  Encodage bit :
//    bit 0 → 1200 Hz  (4 cycles = 8  demi-périodes)
//    bit 1 → 2400 Hz  (8 cycles = 16 demi-périodes)
//  Trame UART par octet :
//    1 start bit (0) | D0..D7 LSB first | 2 stop bits (1 1)
// ============================================================

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <cstring>

namespace KCS {

static constexpr int     SAMPLE_RATE  = 44100;
static constexpr int16_t AMPLITUDE    = 28000;
static constexpr int     LEADER_BITS  = 1500;
static constexpr double  SILENCE_SECS = 0.5;
static constexpr uint8_t MAGIC0 = 0x4D; // 'M'
static constexpr uint8_t MAGIC1 = 0x31; // '1'

// ── Encodage ────────────────────────────────────────────────

static void appendBit(std::vector<int16_t>& samples, int bit, int& level) {
    // bit 0 → 8 demi-périodes @ 1200Hz, bit 1 → 16 demi-périodes @ 2400Hz
    double half_period = (bit == 0) ? (SAMPLE_RATE / 2400.0) : (SAMPLE_RATE / 4800.0);
    int    num_halves  = (bit == 0) ? 8 : 16;
    double frac = 0.0;
    for (int h = 0; h < num_halves; h++) {
        frac += half_period;
        int n = (int)frac;
        frac -= n;
        for (int s = 0; s < n; s++)
            samples.push_back(static_cast<int16_t>(level * AMPLITUDE));
        level = -level;
    }
}

static void appendByte(std::vector<int16_t>& samples, uint8_t byte, int& level) {
    appendBit(samples, 0, level); // start bit
    for (int i = 0; i < 8; i++)
        appendBit(samples, (byte >> i) & 1, level); // D0..D7 LSB first
    appendBit(samples, 1, level); // stop bit 1
    appendBit(samples, 1, level); // stop bit 2
}

// Encode data[0..len-1] en WAV KCS et écrit dans outPath.
// start_addr = adresse de chargement (encodée dans le header KCS).
inline bool encode(const uint8_t* data, size_t len, uint16_t start_addr,
                   const std::string& outPath) {
    std::vector<int16_t> samples;
    samples.reserve(SAMPLE_RATE * 8);
    int level = 1;

    // Silence initial
    int silence_n = (int)(SAMPLE_RATE * SILENCE_SECS);
    for (int i = 0; i < silence_n; i++) samples.push_back(0);

    // Leader (LEADER_BITS x bit-1)
    for (int i = 0; i < LEADER_BITS; i++)
        appendBit(samples, 1, level);

    // Checksum XOR
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; i++) checksum ^= data[i];

    // Header
    appendByte(samples, MAGIC0,                        level);
    appendByte(samples, MAGIC1,                        level);
    appendByte(samples, (uint8_t)(start_addr & 0xFF),  level);
    appendByte(samples, (uint8_t)(start_addr >> 8),    level);
    appendByte(samples, (uint8_t)(len & 0xFF),         level);
    appendByte(samples, (uint8_t)(len >> 8),           level);

    // Données
    for (size_t i = 0; i < len; i++)
        appendByte(samples, data[i], level);

    // Checksum
    appendByte(samples, checksum, level);

    // Silence final
    for (int i = 0; i < silence_n; i++) samples.push_back(0);

    // Écrire le WAV
    std::ofstream f(outPath, std::ios::binary);
    if (!f) return false;

    uint32_t data_size = (uint32_t)(samples.size() * 2);
    uint32_t chunk_size = 36 + data_size;
    uint32_t byte_rate = SAMPLE_RATE * 2;
    uint16_t block_align = 2;
    uint16_t bits = 16;
    uint16_t channels = 1;
    uint32_t sample_rate = SAMPLE_RATE;

    // RIFF header
    f.write("RIFF", 4);
    f.write(reinterpret_cast<const char*>(&chunk_size), 4);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    uint32_t fmt_size = 16; f.write(reinterpret_cast<const char*>(&fmt_size), 4);
    uint16_t pcm = 1;       f.write(reinterpret_cast<const char*>(&pcm), 2);
    f.write(reinterpret_cast<const char*>(&channels), 2);
    f.write(reinterpret_cast<const char*>(&sample_rate), 4);
    f.write(reinterpret_cast<const char*>(&byte_rate), 4);
    f.write(reinterpret_cast<const char*>(&block_align), 2);
    f.write(reinterpret_cast<const char*>(&bits), 2);
    f.write("data", 4);
    f.write(reinterpret_cast<const char*>(&data_size), 4);
    f.write(reinterpret_cast<const char*>(samples.data()), data_size);
    return f.good();
}

// ── Décodage ────────────────────────────────────────────────

struct DecodeResult {
    bool     ok          = false;
    uint16_t start_addr  = 0;
    std::vector<uint8_t> data;
    std::string error;
};

static int readWavSamples(const std::string& path,
                          std::vector<int>& out, int& framerate) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return -1;

    char riff[4]; f.read(riff, 4);
    if (memcmp(riff, "RIFF", 4) != 0) return -2;
    uint32_t chunk_size; f.read(reinterpret_cast<char*>(&chunk_size), 4);
    char wave[4]; f.read(wave, 4);
    if (memcmp(wave, "WAVE", 4) != 0) return -2;

    uint16_t audio_fmt=0, channels=0, bits=0, block_align=0;
    uint32_t sr=0, byte_rate=0;

    while (f) {
        char id[4]; f.read(id, 4);
        uint32_t sz; f.read(reinterpret_cast<char*>(&sz), 4);
        if (!f) break;
        if (memcmp(id, "fmt ", 4) == 0) {
            f.read(reinterpret_cast<char*>(&audio_fmt), 2);
            f.read(reinterpret_cast<char*>(&channels), 2);
            f.read(reinterpret_cast<char*>(&sr), 4);
            f.read(reinterpret_cast<char*>(&byte_rate), 4);
            f.read(reinterpret_cast<char*>(&block_align), 2);
            f.read(reinterpret_cast<char*>(&bits), 2);
            if (sz > 16) f.seekg(sz - 16, std::ios::cur);
        } else if (memcmp(id, "data", 4) == 0) {
            framerate = (int)sr;
            std::vector<char> raw(sz);
            f.read(raw.data(), sz);
            // Convertir en mono 16-bit
            if (bits == 8) {
                for (size_t i = 0; i < sz; i += channels)
                    out.push_back(((uint8_t)raw[i] - 128) * 256);
            } else if (bits == 16) {
                for (size_t i = 0; i < sz; i += channels * 2) {
                    int16_t s; memcpy(&s, &raw[i], 2);
                    out.push_back(s);
                }
            }
            return 0;
        } else {
            f.seekg(sz, std::ios::cur);
        }
    }
    return -3;
}

inline DecodeResult decode(const std::string& wavPath) {
    DecodeResult res;
    std::vector<int> samples;
    int framerate = 0;
    if (readWavSamples(wavPath, samples, framerate) != 0) {
        res.error = "Impossible de lire le fichier WAV";
        return res;
    }

    // Supprimer l'offset DC
    if (!samples.empty()) {
        long long sum = 0;
        for (int s : samples) sum += s;
        int mean = (int)(sum / (long long)samples.size());
        for (int& s : samples) s -= mean;
    }

    // Detection passages par zero avec hysteresis 5% du peak
    int peak = 0;
    for (int s : samples) if (abs(s) > peak) peak = abs(s);
    int hyst = peak / 20;

    std::vector<int> crossings;
    int state = (samples.empty() || samples[0] >= 0) ? 1 : -1;
    for (size_t i = 1; i < samples.size(); i++) {
        if      (state == -1 && samples[i] >  hyst) { crossings.push_back((int)i); state =  1; }
        else if (state ==  1 && samples[i] < -hyst) { crossings.push_back((int)i); state = -1; }
    }

    // Classifier les demi-periodes
    double short_hp  = framerate / 4800.0;
    double long_hp   = framerate / 2400.0;
    double threshold = (short_hp + long_hp) / 2.0;
    double min_gate  = short_hp * 0.4;

    std::vector<int> halves;
    for (size_t i = 1; i < crossings.size(); i++) {
        int d = crossings[i] - crossings[i-1];
        if ((double)d < min_gate) continue;
        halves.push_back(d < threshold ? 1 : 0);
    }

    // Demi-periodes -> bits par run-length
    std::vector<int> bits;
    {
        size_t i = 0;
        while (i < halves.size()) {
            int v = halves[i];
            size_t j = i;
            while (j < halves.size() && halves[j] == v) j++;
            int count = (int)(j - i);
            int nbits = (v == 1) ? (int)round(count / 16.0)
                                 : (int)round(count / 8.0);
            for (int k = 0; k < nbits; k++) bits.push_back(v);
            i = j;
        }
    }

    // readByte : lit un octet UART (start + 8 data LSB + 2 stop) depuis bits[pos].
    // Prend pos PAR REFERENCE et l'avance de 11 en cas de succes.
    // Retourne false sans modifier pos en cas d'echec.
    // IMPORTANT : l'ancien code retournait 0 (size_t) en cas d'echec,
    // ce qui ecrasait pos=0 et causait une boucle infinie.
    auto readByte = [&](size_t& pos, uint8_t& out) -> bool {
        if (pos + 11 > bits.size()) return false;
        if (bits[pos]    != 0) return false;  // start bit manquant
        if (bits[pos+9]  != 1) return false;  // stop bit 1 manquant
        if (bits[pos+10] != 1) return false;  // stop bit 2 manquant
        uint8_t b = 0;
        for (int k = 0; k < 8; k++) b |= bits[pos+1+k] << k;
        out = b;
        pos += 11;
        return true;
    };

    // Chercher le leader puis decoder le bloc
    size_t n = bits.size();
    size_t i = 0;
    while (i < n) {
        if (bits[i] != 1) { i++; continue; }

        // Mesurer le leader (suite de bits-1)
        size_t leader_start = i;
        while (i < n && bits[i] == 1) i++;
        if (i - leader_start < 100) continue; // trop court, pas un vrai leader

        // Tenter de decoder depuis i (premier bit apres le leader)
        size_t pos = i;
        uint8_t b, lo, hi;

        // Magic 'M' (0x4D) '1' (0x31)
        if (!readByte(pos, b) || b != MAGIC0) continue;
        if (!readByte(pos, b) || b != MAGIC1) continue;

        // Adresse de chargement little-endian
        if (!readByte(pos, lo)) continue;
        if (!readByte(pos, hi)) continue;
        uint16_t start_addr = lo | ((uint16_t)hi << 8);

        // Longueur little-endian
        if (!readByte(pos, lo)) continue;
        if (!readByte(pos, hi)) continue;
        uint16_t length = lo | ((uint16_t)hi << 8);
        if (length == 0 || length > 0x8000) continue;

        // Donnees + checksum XOR en vol
        std::vector<uint8_t> data;
        data.reserve(length);
        uint8_t checksum = 0;
        bool ok = true;
        for (int k = 0; k < (int)length; k++) {
            if (!readByte(pos, b)) { ok = false; break; }
            data.push_back(b);
            checksum ^= b;
        }
        if (!ok) continue;

        // Octet de checksum
        uint8_t stored_cs;
        if (!readByte(pos, stored_cs)) continue;
        if (stored_cs != checksum) {
            res.error = "Checksum KCS invalide (fichier corrompu ?)";
            continue;
        }

        // Succes
        res.ok         = true;
        res.start_addr = start_addr;
        res.data       = std::move(data);
        return res;
    }

    if (res.error.empty()) res.error = "Aucun bloc KCS valide trouve dans le fichier";
    return res;
}

} // namespace KCS
