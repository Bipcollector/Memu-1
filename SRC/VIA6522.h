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
#include <cstdint>
#include <array>

class VIA6522 {
public:
    VIA6522(); 
    ~VIA6522() = default;

    uint8_t read(uint8_t reg);
    void write(uint8_t reg, uint8_t data);

    void setJoystick0(uint8_t s) { joy0 = s; }
    void setJoystick1(uint8_t s) { joy1 = s; }
    [[nodiscard]] bool isBuzzerOn() const { return buzzer; }

    // Vrai seulement quand ACR bits 7-6 = 11 : le Timer 1 pilote PB7
    // en mode auto-toggle (= TONE actif). Permet à l'AudioEngine
    // d'émettre du silence (0) plutôt que -AMPLITUDE quand aucun son
    // n'est demandé, évitant le craquement au démarrage.
    [[nodiscard]] bool isACRAutoToggle() const { return (regs[11] & 0xC0) == 0xC0; }

    [[nodiscard]] bool hasInterrupt() const {
        return (regs[13] & regs[14] & 0x7F) != 0;
    }

    // Timer 1 : génère l'horloge du buzzer (PB7) en continu.
    //
    // Sur le vrai 6522, quand ACR (registre 11) a ses bits 7-6 à "11",
    // le Timer 1 pilote directement la broche PB7 EN MATÉRIEL : à
    // chaque underflow (passage à zéro), la broche PB7 bascule
    // automatiquement (0->1 ou 1->0), SANS aucune intervention du
    // CPU. C'est ce mécanisme, et non une écriture logicielle directe
    // sur le port B, que la ROM du Memo-1 utilise pour produire un
    // son : elle configure juste la fréquence via T1 et active ACR,
    // le VIA fait le reste tout seul.
    void tick(uint16_t cycles) {
        if (timer1_counter > 0) {
            if (timer1_counter <= cycles) {
                // Underflow : recharger le compteur avec le "latch"
                // (T1L-L/T1L-H) pour simuler un signal d'horloge
                // carré continu, ET basculer PB7 si le mode
                // matériel auto-toggle est actif (ACR bits 7-6 = 11).
                timer1_counter = (static_cast<uint16_t>(regs[7]) << 8) | regs[6];
                if ((regs[11] & 0xC0) == 0xC0) {
                    buzzer = !buzzer;
                }
            } else {
                timer1_counter -= cycles;
            }
        }
    }

private:
    std::array<uint8_t, 16> regs{};
    uint8_t joy0 = 0x1F, joy1 = 0x1F;
    bool buzzer = false;
    uint16_t timer1_counter = 0;
};