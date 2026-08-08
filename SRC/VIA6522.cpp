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
        // T1CH : Écrire ici charge le compteur ET lance le comptage.
        // Sur le vrai 6522, écrire T1CH charge AUSSI le latch T1LH (reg 7) :
        // sans ça, au premier underflow le timer se recharge à 0 (latch vide),
        // PB7 se bloque sur son dernier état, et on entend des craquements
        // au lieu d'une oscillation continue.
        regs[5] = data;
        regs[7] = data;  // T1LH = T1CH (datasheet 6522 §7.2)
        // T1CL a déjà été écrit dans regs[4] ; le latch bas (reg 6) est mis
        // à jour séparément quand on écrit T1LL directement (reg 6).
        // On copie aussi regs[4] dans regs[6] pour que le latch complet
        // soit cohérent dès la première écriture T1CH.
        regs[6] = regs[4];  // T1LL = T1CL
        timer1_counter = (static_cast<uint16_t>(data) << 8) | regs[4];
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
    if (reg == 11) {
        // ACR : si le mode "PB7 auto-toggle" (bits 7-6 = 11) est
        // désactivé, on éteint proprement le buzzer plutôt que de le
        // laisser bloqué sur son dernier état (sinon un son continu
        // resterait audible indéfiniment après coupure).
        bool wasAutoMode = (regs[11] & 0xC0) == 0xC0;
        bool isAutoMode  = (data & 0xC0) == 0xC0;
        regs[11] = data;
        if (wasAutoMode && !isAutoMode) {
            buzzer = (regs[0] & 0x80) != 0;  // retour à l'état "GPIO normal"
        }
        return;
    }
    regs[reg] = data;
}