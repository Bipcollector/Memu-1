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

    [[nodiscard]] bool hasInterrupt() const {
        return (regs[13] & regs[14] & 0x7F) != 0;
    }

    // Timer silencieux : Il tourne pour générer l'horloge, mais ne déclenche pas d'IRQ
    void tick(uint16_t cycles) {
        if (timer1_counter > 0) {
            if (timer1_counter <= cycles) {
                // AU LIEU DE LEVER LE DRAPEAU, ON RECHARGE LE TIMER AVEC SA VALEUR INITIALE
                // Cela simule un signal d'horloge carré continu sans submerger le CPU !
                timer1_counter = (static_cast<uint16_t>(regs[7]) << 8) | regs[6];
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