#pragma once
#include <cstdint>
#include <array>
#include <string>
#include "ACIA6551.h"
#include "VIA6522.h"

class Bus {
public:
    Bus();
    ~Bus() = default;

    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t data);

    ACIA6551& getACIA() { return acia; }
    VIA6522&  getVIA()  { return via; }
    std::array<uint8_t, 16384>& getROM() { return rom; }

    // ---- Gestion cartouche ($A000-$BFFF, 8 Ko) ----
    // Charge un fichier .bin comme cartouche externe.
    // Retourne vrai si succès, faux sinon.
    bool loadCartridge(const std::string& path);

    // Déconnecte la cartouche (slot vide = bus flottant).
    void ejectCartridge();

    // Vrai si une cartouche est connectée.
    bool hasCartridge() const { return cartridge_loaded; }

    // Nom lu aux 8 premiers octets de la cartouche ($A000-$A007).
    std::string cartridgeName() const;

private:
    std::array<uint8_t, 32768> ram;   // $0000-$7FFF
    std::array<uint8_t, 16384> rom;   // $C000-$FFFF

    // Zone cartouche : 8 Ko ($A000-$BFFF)
    std::array<uint8_t, 8192> cartridge;
    bool cartridge_loaded = false;

    ACIA6551 acia;  // $9000-$9FFF
    VIA6522  via;   // $8000-$8FFF
};
