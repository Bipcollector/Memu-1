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
#include <string>
#include <vector>
#include "ACIA6551.h"
#include "VIA6522.h"

class Bus {
public:
    Bus();
    ~Bus() = default;

    uint8_t read(uint16_t address);
    void    write(uint16_t address, uint8_t data);

    ACIA6551& getACIA() { return acia; }
    VIA6522&  getVIA()  { return via; }
    std::array<uint8_t, 16384>& getROM() { return rom; }

    // ---- Cartouche ($A000-$BFFF, 8 Ko) ----
    bool        loadCartridge(const std::string& path);
    void        ejectCartridge();
    bool        hasCartridge() const { return cartridge_loaded; }
    std::string cartridgeName() const;

    // ---- ROM overlay KCS ----
    // Remplace E389 (routine KCS receive hardware) par CLC+RTS.
    // On écrit les données en RAM et les variables ZP AVANT l'appel,
    // la ROM reprend à F955 (BCS pas pris) et affiche "Loaded.".
    void enableKCSOverlay(uint16_t start_addr, uint16_t length) {
        kcs_overlay = true;
        // $0200/$0201 = adresse affichée par la ROM après LOADED
        ram[0x0200] = start_addr & 0xFF;
        ram[0x0201] = start_addr >> 8;
        // $F5/$F6 = start, $F7/$F8 = length (utilisés par F957+)
        ram[0x00F5] = start_addr & 0xFF;
        ram[0x00F6] = start_addr >> 8;
        ram[0x00F7] = length & 0xFF;
        ram[0x00F8] = length >> 8;
    }
    void disableKCSOverlay() { kcs_overlay = false; }

    // tickKCS() conservé pour compatibilité mais inutilisé avec overlay
    void tickKCS() {}

private:
    std::array<uint8_t, 32768> ram;
    std::array<uint8_t, 16384> rom;
    std::array<uint8_t, 8192>  cartridge;
    bool cartridge_loaded = false;

    ACIA6551 acia;
    VIA6522  via;

    // ROM overlay : quand actif, Bus::read(E389..E38A) retourne CLC+RTS
    bool kcs_overlay = false;
};
