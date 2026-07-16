#include "VIA6522.h"

VIA6522::VIA6522() {
    regs[2] = 0; regs[3] = 0; regs[0] = 0; regs[1] = 0;
    regs[13] = 0x00; // Aucune interruption au démarrage
    regs[14] = 0x00;
}

uint8_t VIA6522::read(uint8_t reg) {
    reg &= 0x0F;
    if (reg == 0) return (regs[0] & regs[2]) | (joy1 & ~regs[2]);
    if (reg == 1) return (regs[1] & regs[3]) | (joy0 & ~regs[3]);
    if (reg == 13) return (regs[13] & 0x7F) ? (regs[13] | 0x80) : 0x00;
    if (reg == 14) return regs[14] | 0x80;
    if (reg == 5) { regs[13] &= ~0x40; return regs[5]; } // T1CH efface le flag
    if (reg == 9) { regs[13] &= ~0x20; return regs[9]; } // T2CH efface le flag
    return regs[reg];
}

void VIA6522::write(uint8_t reg, uint8_t data) {
    reg &= 0x0F;
    if (reg <= 3) { 
        regs[reg] = data; 
        if (reg == 0 && (regs[2] & 0x80)) buzzer = (data & 0x80) != 0; 
        return; 
    }
    
    // Gestion du Timer 1
    if (reg == 4) { regs[4] = data; return; } // T1CL (Low byte du Latch)
    if (reg == 5) { 
        // T1CH : Écrire ici charge le compteur ET lance le comptage !
        regs[5] = data; 
        // On convertit les deux octets (regs[4] et regs[5]) en un compteur 16 bits
        timer1_counter = (static_cast<uint16_t>(data) << 8) | regs[4];
        // ON NE LEVE PLUS LE DRAPEAU ICI ! La méthode tick() s'en chargera quand le temps sera écoulé.
        return; 
    }
    if (reg == 6 || reg == 7) { regs[reg] = data; return; } // T1LL / T1LH

    // Gestion du Timer 2 (on le laisse en instantané, il est moins critique)
    if (reg == 8) { regs[8] = data; return; }
    if (reg == 9) { regs[9] = data; regs[13] |= 0x20; return; }

    if (reg == 13) { regs[13] &= ~data; return; }
    if (reg == 14) { 
        if (data & 0x80) regs[14] |= (data & 0x7F);
        else regs[14] &= ~(data & 0x7F);
        return; 
    }
    regs[reg] = data;
}