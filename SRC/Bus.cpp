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

#include "Bus.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

Bus::Bus() {
    ram.fill(0x00);
    rom.fill(0xFF);
    cartridge.fill(0x00);
}

uint8_t Bus::read(uint16_t address) {
    if (address <= 0x7FFF) {
        return ram[address];
    }
    else if (address <= 0x8FFF) {
        return via.read(address & 0x0F);
    }
    else if (address <= 0x9FFF) {
        return acia.read(address & 0x03);
    }
    else if (address <= 0xBFFF) {
        // ROM overlay KCS : remplace E389-E38A par CLC(18) + RTS(60)
        // La ROM appelle JSR E389 depuis F952. Avec l'overlay actif,
        // elle exécute CLC puis RTS -> retour à F955 avec carry=0
        // -> BCS F9BD non pris -> affiche "Loaded." normalement.
        if (kcs_overlay) {
            if (address == 0xE389) return 0x18;  // CLC
            if (address == 0xE38A) return 0x60;  // RTS
        }
        if (cartridge_loaded) {
            return cartridge[address & 0x1FFF];
        } else {
            return (address >> 8) & 0xFF;
        }
    }
    else {
        // ROM overlay couvre aussi E389 qui est dans la ROM (C000-FFFF)
        if (kcs_overlay) {
            if (address == 0xE389) return 0x18;  // CLC
            if (address == 0xE38A) return 0x60;  // RTS
        }
        return rom[address - 0xC000];
    }
}

void Bus::write(uint16_t address, uint8_t data) {
    if (address <= 0x7FFF) {
        ram[address] = data;
    }
    else if (address <= 0x8FFF) {
        via.write(address & 0x0F, data);
    }
    else if (address <= 0x9FFF) {
        acia.write(address & 0x03, data);
    }
    // ROM ($C000-$FFFF) et cartouche ($A000-$BFFF) : lecture seule
}

bool Bus::loadCartridge(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto size = f.tellg();
    if (size <= 0 || size > 8192) return false;
    f.seekg(0);
    cartridge.fill(0x00);
    f.read(reinterpret_cast<char*>(cartridge.data()), size);
    cartridge_loaded = true;
    return true;
}

void Bus::ejectCartridge() {
    cartridge.fill(0x00);
    cartridge_loaded = false;
}

std::string Bus::cartridgeName() const {
    if (!cartridge_loaded) return "";
    std::string name;
    for (int i = 0; i < 8; i++) {
        uint8_t c = cartridge[i];
        if (c >= 0x20 && c < 0x7F) name += static_cast<char>(c);
        else break;
    }
    return name;
}
