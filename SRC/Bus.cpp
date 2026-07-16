#include "Bus.h"
#include <fstream>
#include <algorithm>

Bus::Bus() {
    ram.fill(0x00);
    rom.fill(0x00);
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
        if (cartridge_loaded) {
            // Cartouche connectée : on lit dans les 8 Ko de cartouche.
            // Le slot fait 8 Ko, on masque sur 0x1FFF pour les miroirs éventuels.
            return cartridge[address & 0x1FFF];
        } else {
            // Bus flottant : renvoie le poids fort de l'adresse.
            // C'est ce que la ROM Memo-1 teste pour détecter un slot vide :
            //   LDA $A000 / CMP #$A0 / BEQ slot_vide
            return (address >> 8) & 0xFF;
        }
    }
    else {
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
    // La ROM ($C000-$FFFF) et la cartouche ($A000-$BFFF) sont en lecture seule.
}

bool Bus::loadCartridge(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;

    std::streamsize size = f.tellg();
    if (size <= 0 || size > 8192) return false;

    f.seekg(0, std::ios::beg);
    cartridge.fill(0x00);
    f.read(reinterpret_cast<char*>(cartridge.data()), size);
    if (!f) return false;

    cartridge_loaded = true;
    return true;
}

void Bus::ejectCartridge() {
    cartridge.fill(0x00);
    cartridge_loaded = false;
}

std::string Bus::cartridgeName() const {
    if (!cartridge_loaded) return "";
    // Les 8 premiers octets sont le nom de la cartouche
    std::string name;
    for (int i = 0; i < 8; i++) {
        uint8_t c = cartridge[i];
        if (c >= 0x20 && c < 0x7F) {
            name += static_cast<char>(c);
        } else {
            name += ' ';
        }
    }
    // Supprimer les espaces de fin
    while (!name.empty() && name.back() == ' ') name.pop_back();
    return name;
}
