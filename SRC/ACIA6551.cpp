#include "ACIA6551.h"
#include <iostream>
#include <sstream>
#include <vector>

// ============================================================
//  Interpréteur Videotex Minitel complet
//  Référence : STUM 1B Partie 2 Chapitre 2 "L'écran"
//
//  On traduit les séquences Videotex vers des séquences ANSI
//  standard (ISO 6429) supportées par le terminal Windows 10+.
//  Il suffit d'activer le mode ANSI dans la console Windows :
//  main.cpp appelle SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING).
// ============================================================

// Mapping couleur Videotex (0..7) -> code couleur ANSI (30..37 / 40..47)
static const int VT_FG[8] = { 30,31,32,33,34,35,36,37 };
static const int VT_BG[8] = { 40,41,42,43,44,45,46,47 };

// Blocs Unicode pour le jeu G1 semi-graphique Minitel
// Un caractère G1 code 6 zones (2 colonnes x 3 lignes) plus un bit "séparateur"
// La meilleure approximation dans un terminal texte monospace :
// on utilise les caractères de demi-blocs Unicode (U+2580..U+259F)
// et les quadrants (U+2596..U+259F).
//
// Le code G1 est (byte & 0x7F) - 0x20 pour obtenir un index 0..95.
// Bits du byte G1 (après masque & 0x7F, base 0x20) :
//   b0 = zone haut-gauche    b1 = zone haut-droite
//   b2 = zone mil-gauche     b3 = zone mil-droite
//   b4 = zone bas-gauche     b5 = zone bas-droite
//   b6 = séparateur (jointif/disjoint), on l'ignore pour l'affichage texte
//
// Dans un terminal monospace, on n'a que 2 lignes par caractère (haut/bas).
// On approxime en combinant haut-gauche|mil-gauche -> gauche haute,
// bas-gauche -> gauche basse, etc. -> quadrant Unicode.
// Mapping vers les 16 quadrants Unicode U+2596-U+259F + espaces et pleins.

static const char* G1_UNICODE[64] = {
//  hg hd mg md bg bd  -> valeur bits 0-5 (bit6=separateur ignore)
    " ",        // 000000 : vide
    "\xe2\x96\x98", // 000001 hg       : ▘
    "\xe2\x96\x9d", // 000010 hd       : ▝
    "\xe2\x96\x80", // 000011 hg+hd    : ▀  demi haut
    "\xe2\x96\x96", // 000100 mg       : ▖ (approx bas-gauche)
    "\xe2\x96\x8c", // 000101 hg+mg    : ▌ demi gauche
    "\xe2\x96\x9e", // 000110 hd+mg    : ▞
    "\xe2\x96\x9b", // 000111          : ▛
    "\xe2\x96\x97", // 001000 md       : ▗
    "\xe2\x96\x9a", // 001001 hg+md    : ▚
    "\xe2\x96\x90", // 001010 hd+md    : ▐ demi droite
    "\xe2\x96\x9c", // 001011          : ▜
    "\xe2\x96\x84", // 001100 mg+md    : ▄ demi bas
    "\xe2\x96\x99", // 001101          : ▙
    "\xe2\x96\x9f", // 001110          : ▟
    "\xe2\x96\x88", // 001111 hg+hd+mg+md : █ plein (haut+mil)
    "\xe2\x96\x96", // 010000 bg       : ▖
    "\xe2\x96\x9e", // 010001 hg+bg    : ▞ (approx)
    "\xe2\x96\x9d", // 010010 hd+bg    : ▝ (approx)
    "\xe2\x96\x9b", // 010011          : ▛
    "\xe2\x96\x8c", // 010100 mg+bg    : ▌
    "\xe2\x96\x88", // 010101 hg+mg+bg : █
    "\xe2\x96\x90", // 010110          : ▐
    "\xe2\x96\x88", // 010111          : █
    "\xe2\x96\x84", // 011000 md+bg    : ▄
    "\xe2\x96\x99", // 011001          : ▙
    "\xe2\x96\x9f", // 011010          : ▟
    "\xe2\x96\x88", // 011011          : █
    "\xe2\x96\x88", // 011100 mg+md+bg : █
    "\xe2\x96\x88", // 011101          : █
    "\xe2\x96\x88", // 011110          : █
    "\xe2\x96\x88", // 011111          : █ plein
    "\xe2\x96\x97", // 100000 bd       : ▗
    "\xe2\x96\x9a", // 100001 hg+bd    : ▚
    "\xe2\x96\x90", // 100010 hd+bd    : ▐
    "\xe2\x96\x9c", // 100011          : ▜
    "\xe2\x96\x84", // 100100 mg+bd    : ▄
    "\xe2\x96\x99", // 100101          : ▙
    "\xe2\x96\x88", // 100110          : █
    "\xe2\x96\x88", // 100111          : █
    "\xe2\x96\x84", // 101000 md+bd    : ▄
    "\xe2\x96\x88", // 101001          : █
    "\xe2\x96\x90", // 101010 hd+md+bd : ▐
    "\xe2\x96\x88", // 101011          : █
    "\xe2\x96\x84", // 101100 mg+md+bd : ▄
    "\xe2\x96\x88", // 101101          : █
    "\xe2\x96\x88", // 101110          : █
    "\xe2\x96\x88", // 101111          : █ plein
    "\xe2\x96\x84", // 110000 bg+bd    : ▄ demi bas
    "\xe2\x96\x99", // 110001          : ▙
    "\xe2\x96\x9f", // 110010          : ▟
    "\xe2\x96\x88", // 110011          : █
    "\xe2\x96\x88", // 110100          : █
    "\xe2\x96\x88", // 110101          : █
    "\xe2\x96\x88", // 110110          : █
    "\xe2\x96\x88", // 110111          : █
    "\xe2\x96\x88", // 111000          : █
    "\xe2\x96\x88", // 111001          : █
    "\xe2\x96\x88", // 111010          : █
    "\xe2\x96\x88", // 111011          : █
    "\xe2\x96\x88", // 111100          : █
    "\xe2\x96\x88", // 111101          : █
    "\xe2\x96\x88", // 111110          : █
    "\xe2\x96\x88", // 111111 tous bits: █ plein
};

// ============================================================
//  Constructeur
// ============================================================
ACIA6551::ACIA6551() {
    status  = TDRE;
    command = 0x02;
    // Couleurs initiales Minitel : blanc sur noir
    fg_color = 7;
    bg_color = 0;
}

// ============================================================
//  Registres ACIA
// ============================================================
void ACIA6551::pollKeyboard() {
    if (!keyboard_buffer.empty()) status |=  RDRF;
    else                          status &= ~RDRF;
}

uint8_t ACIA6551::read(uint8_t reg) {
    switch (reg & 0x03) {
        case 0x00:
            if (!keyboard_buffer.empty()) {
                rx_data = keyboard_buffer.front();
                keyboard_buffer.pop();
            }
            status = keyboard_buffer.empty() ? (status & ~RDRF) : (status | RDRF);
            return rx_data;
        case 0x01: return status | TDRE;
        case 0x02: return command;
        case 0x03: return control;
        default:   return 0x00;
    }
}

void ACIA6551::write(uint8_t reg, uint8_t data) {
    switch (reg & 0x03) {
        case 0x00:
            tx_data = data;
            processNormal(data);   // dispatcher principal
            break;
        case 0x01: status &= ~(PE | FE | OVRN); break;
        case 0x02: command = data; break;
        case 0x03: control = data; break;
    }
}

// ============================================================
//  Flush vers stdout
// ============================================================
void ACIA6551::flushDisplay() {
    if (!display_buf.empty()) {
        std::cout << display_buf;
        std::cout.flush();
        display_buf.clear();
    }
}

// ============================================================
//  Émettre les attributs ANSI courants
// ============================================================
void ACIA6551::emitAttrs() {
    // On ne réémet la séquence ANSI QUE si les attributs ont réellement
    // changé depuis la dernière fois. Sans ça, chaque caractère (même un
    // simple chiffre répété) déclenchait un "\033[0;37;40m" complet,
    // ce qui multipliait le volume de données envoyées au terminal par
    // 8-10x et pouvait saturer l'I/O de la vraie console Windows.
    if (attrs_emitted_once &&
        emitted_fg  == fg_color &&
        emitted_bg  == bg_color &&
        emitted_inv == inverse &&
        emitted_und == underline) {
        return;  // rien n'a changé : ne rien émettre
    }

    std::string s = "\033[0;";
    s += std::to_string(inverse ? VT_BG[fg_color] : VT_FG[fg_color]);
    s += ";";
    s += std::to_string(inverse ? VT_FG[bg_color] : VT_BG[bg_color]);
    if (underline) s += ";4";
    s += "m";
    emit(s);

    emitted_fg  = fg_color;
    emitted_bg  = bg_color;
    emitted_inv = inverse;
    emitted_und = underline;
    attrs_emitted_once = true;
}

void ACIA6551::resetAttrs() {
    fg_color  = 7;
    bg_color  = 0;
    inverse   = false;
    underline = false;
    charset   = CharSet::G0;
    emit("\033[0m");
    // Le \033[0m remet le terminal à l'état par défaut : on synchronise
    // le cache pour refléter cet état (évite un emitAttrs() redondant
    // juste après un reset).
    emitted_fg  = 7;
    emitted_bg  = 0;
    emitted_inv = false;
    emitted_und = false;
    attrs_emitted_once = true;
}

// ============================================================
//  Dispatcher principal (état NORMAL)
// ============================================================
void ACIA6551::processNormal(uint8_t b) {
    // Si on est dans un état intermédiaire, on délègue
    switch (state) {
        case State::ESC:  processEsc(b); return;
        case State::CSI:  processCsi(b); return;
        case State::SS2:  processSS2(b); return;
        case State::US1:
        case State::US2:  processUS(b);  return;
        case State::ESC_PRO:
            // Accumule les bytes de la séquence PRO1/PRO2/PRO3
            if (pro_idx < 3) pro_buf[pro_idx++] = b;
            if (pro_idx >= pro_total) {
                // Séquence complète : interpréter
                // PRO2 (total=2) : pro_buf[0]=cmd, pro_buf[1]=param
                // ESC 0x3A 0x69 0x45 = PRO2 START ROULEAU -> mode scroll
                // ESC 0x3A 0x6A 0x45 = PRO2 STOP  ROULEAU -> mode page
                if (pro_total == 2 && pro_buf[1] == 0x45) {
                    if (pro_buf[0] == 0x69) {
                        // START ROULEAU : quitter l'écran alternatif -> scroll actif
                        mode_rouleau = true;
                        emit("\033[?1049l");   // retour terminal normal (avec scroll)
                        emit("\033[?25h");      // curseur visible en mode BASIC
                    } else if (pro_buf[0] == 0x6A) {
                        // STOP ROULEAU (= mode page) : repasser en écran alternatif
                        mode_rouleau = false;
                        emit("\033[?1049h");   // écran alternatif (sans scroll)
                    }
                }
                state = State::NORMAL;
            }
            return;
        default: break;
    }

    // --- Codes C0 (contrôle) ---
    switch (b) {
        case 0x00: return;  // NUL : ignoré
        case 0x07: return;  // BEL : sonnerie (ignorée)
        case 0x08: emit("\033[D"); return;  // BS  : curseur gauche
        case 0x09: emit("\033[C"); return;  // HT  : curseur droite
        case 0x0A:
            // LF : comportement différent selon le mode d'affichage.
            //
            // Mode PAGE (Minitel par défaut) : le curseur descend d'une
            // ligne SANS jamais faire défiler l'écran (§1.2.5.3 du PDF :
            // "seules les rangées 01 et 24 sont concernées" en mode
            // rouleau, mais en mode page il n'y a AUCUN défilement).
            // On utilise \033[B (déplacement pur, sans scroll).
            //
            // Mode ROULEAU (activé par PRO2 START ROULEAU, ex: MS-BASIC) :
            // le terminal doit défiler naturellement quand on est en bas
            // de l'écran. \033[B ne fait JAMAIS défiler un terminal ANSI
            // (ce n'est qu'un déplacement de curseur, clampé en bas) :
            // il faut émettre un VRAI saut de ligne '\n' pour que le
            // terminal Windows déclenche son propre mécanisme de scroll.
            if (mode_rouleau) {
                emit("\n");
            } else {
                emit("\033[B");
            }
            return;
        case 0x0B: emit("\033[A"); return;  // VT  : curseur haut
        case 0x0C:                           // FF  : clear screen
            emit("\033[2J\033[H");
            resetAttrs();
            return;
        case 0x0D:
            emit("\r");                              // CR
            if (capturing) capture_buf += '\n';       // capture : fin de ligne
            return;
        case 0x0E:                           // SO : jeu G1 (semi-graphique)
            charset = CharSet::G1;
            return;
        case 0x0F:                           // SI : retour G0
            charset = CharSet::G0;
            return;
        case 0x11: emit("\033[?25h"); return; // CON  : curseur visible
        case 0x14: emit("\033[?25l"); return; // COFF : curseur invisible
        case 0x18:                            // CAN : efface fin de rangée
            emit("\033[K");
            return;
        case 0x19:                            // SS2 : prochain char = G2
            state = State::SS2;
            return;
        case 0x1B:                            // ESC
            state = State::ESC;
            return;
        case 0x1E:                            // RS : séparateur article -> col 01 rangée 01
            emit("\033[1;1H");
            resetAttrs();
            return;
        case 0x1F:                            // US : sous-article/positionnement
            state = State::US1;
            return;
        default:
            if (b < 0x20) return;  // autres codes de contrôle : ignorés
            break;
    }

    // --- Caractères imprimables (>= 0x20) ---
    if (charset == CharSet::G1) {
        // Jeu semi-graphique
        emitAttrs();
        emit(g1ToUtf8(b));
    } else {
        // Jeu G0 (ASCII standard)
        // Certains codes Videotex diffèrent de l'ASCII :
        // 0x23 -> £ (livre sterling en G0 Minitel)  -> on garde '#' pour simplifier
        // 0x40 -> à
        // 0x5B -> °, 0x5C -> ç, 0x5D -> §, 0x7B -> é, 0x7C -> ù, 0x7D -> è, 0x7E -> ¨
        emitAttrs();
        // Capture : on garde l'octet BRUT reçu (pas sa traduction UTF-8),
        // car c'est cet octet-là qu'il faudra réinjecter tel quel lors
        // d'un LOAD ultérieur pour reproduire le même texte.
        if (capturing) capture_buf += static_cast<char>(b);
        switch (b) {
            case 0x40: emit("\xc3\xa0"); break; // à
            case 0x5B: emit("\xc2\xb0"); break; // °
            case 0x5C: emit('\\'); break;         // backslash
            case 0x5D: emit("\xc2\xa7"); break; // §
            case 0x7B: emit("\xc3\xa9"); break; // é
            case 0x7C: emit('|'); break;           // pipe
            case 0x7D: emit("\xc3\xa8"); break; // è
            case 0x7E: emit("\xc2\xa8"); break; // ¨
            default:
                if (b >= 0x20 && b < 0x7F) emit(static_cast<char>(b));
                break;
        }
    }
}

// ============================================================
//  Traitement séquence ESC
// ============================================================
void ACIA6551::processEsc(uint8_t b) {
    state = State::NORMAL;

    if (b == 0x5B) {
        // ESC [ -> CSI (ISO 6429)
        csi_params.clear();
        state = State::CSI;
        return;
    }

    // Commandes protocole Télétel (§1.2.7) : avaler N bytes sans afficher
    // ESC 0x39 = PRO1 (1 byte suivant)
    // ESC 0x3A = PRO2 (2 bytes suivants)
    // ESC 0x3B = PRO3 (3 bytes suivants)
    if (b == 0x39) { pro_total=1; pro_idx=0; state = State::ESC_PRO; return; }
    if (b == 0x3A) { pro_total=2; pro_idx=0; state = State::ESC_PRO; return; }
    if (b == 0x3B) { pro_total=3; pro_idx=0; state = State::ESC_PRO; return; }

    // Grille C1 : col 4 (0x40-0x4F) et col 5 (0x50-0x5F)
    switch (b) {
        // --- Couleur du caractère (col 4, rangées 0-7) ---
        case 0x40: fg_color = 0; emitAttrs(); return;  // Noir
        case 0x41: fg_color = 1; emitAttrs(); return;  // Rouge
        case 0x42: fg_color = 2; emitAttrs(); return;  // Vert
        case 0x43: fg_color = 3; emitAttrs(); return;  // Jaune
        case 0x44: fg_color = 4; emitAttrs(); return;  // Bleu
        case 0x45: fg_color = 5; emitAttrs(); return;  // Magenta
        case 0x46: fg_color = 6; emitAttrs(); return;  // Cyan
        case 0x47: fg_color = 7; emitAttrs(); return;  // Blanc
        // --- Clignotement (col 4, rangées 8-9) ---
        case 0x48: emit("\033[5m"); return;  // Clignotement ON
        case 0x49: emit("\033[25m"); return; // Clignotement OFF (fixe)
        // --- Taille (col 4, rangées A-D) : ignoré en mode texte ---
        case 0x4A: return; // Grandeur normale
        case 0x4B: return; // Double hauteur
        case 0x4C: return; // Double largeur
        case 0x4D: return; // Double taille
        // --- Inversion fond (col 4, rangées E-F) ---
        case 0x4E: inverse = false; emitAttrs(); return; // Fond normal
        case 0x4F: inverse = true;  emitAttrs(); return; // Fond inversé
        // --- Couleur du fond (col 5, rangées 0-7) ---
        case 0x50: bg_color = 0; emitAttrs(); return;  // Fond Noir
        case 0x51: bg_color = 1; emitAttrs(); return;  // Fond Rouge
        case 0x52: bg_color = 2; emitAttrs(); return;  // Fond Vert
        case 0x53: bg_color = 3; emitAttrs(); return;  // Fond Jaune
        case 0x54: bg_color = 4; emitAttrs(); return;  // Fond Bleu
        case 0x55: bg_color = 5; emitAttrs(); return;  // Fond Magenta
        case 0x56: bg_color = 6; emitAttrs(); return;  // Fond Cyan
        case 0x57: bg_color = 7; emitAttrs(); return;  // Fond Blanc
        // --- Masquage / lignage (col 5, rangées 8-A) ---
        case 0x58: return; // Début masquage (ignoré)
        case 0x59: underline = false; emitAttrs(); return; // Fin lignage
        case 0x5A: underline = true;  emitAttrs(); return; // Début lignage
        // --- Taille double (col 5, rangées C-F) : ignoré en mode texte ---
        case 0x5C: return; // Grandeur normale
        case 0x5D: return; // Double hauteur
        case 0x5E: return; // Double largeur
        case 0x5F: return; // Démasquage
        // Tout le reste : séquence non définie, ignorée (§1.2.7 du PDF)
        default: return;
    }
}

// ============================================================
//  Traitement séquence CSI (ESC [)
// ============================================================
void ACIA6551::processCsi(uint8_t b) {
    // Accumule les digits et ';' jusqu'au byte final (0x40-0x7E)
    if ((b >= '0' && b <= '9') || b == ';') {
        csi_params += static_cast<char>(b);
        return;
    }

    // Byte final reçu -> on parse les paramètres
    state = State::NORMAL;

    // Parser les paramètres numériques (séparés par ';')
    auto getParams = [&]() -> std::vector<int> {
        std::vector<int> v;
        std::istringstream ss(csi_params);
        std::string tok;
        while (std::getline(ss, tok, ';')) {
            if (tok.empty()) v.push_back(0);
            else             v.push_back(std::stoi(tok));
        }
        if (v.empty()) v.push_back(0);
        return v;
    };

    switch (b) {
        case 'A': { // Curseur haut Pn
            auto p = getParams();
            int n = (p[0] == 0) ? 1 : p[0];
            emit("\033[" + std::to_string(n) + "A");
            break;
        }
        case 'B': { // Curseur bas Pn
            auto p = getParams();
            int n = (p[0] == 0) ? 1 : p[0];
            emit("\033[" + std::to_string(n) + "B");
            break;
        }
        case 'C': { // Curseur droite Pn
            auto p = getParams();
            int n = (p[0] == 0) ? 1 : p[0];
            emit("\033[" + std::to_string(n) + "C");
            break;
        }
        case 'D': { // Curseur gauche Pn
            auto p = getParams();
            int n = (p[0] == 0) ? 1 : p[0];
            emit("\033[" + std::to_string(n) + "D");
            break;
        }
        case 'H': { // Positionnement direct : ESC [ Pr ; Pc H
            auto p = getParams();
            int row = (p.size() >= 1 && p[0] > 0) ? p[0] : 1;
            int col = (p.size() >= 2 && p[1] > 0) ? p[1] : 1;
            emit("\033[" + std::to_string(row) + ";" + std::to_string(col) + "H");
            break;
        }
        case 'J': { // Effacement écran
            auto p = getParams();
            switch (p[0]) {
                case 0: emit("\033[0J"); break; // depuis curseur -> fin
                case 1: emit("\033[1J"); break; // début -> curseur
                case 2:                          // tout l'écran
                    emit("\033[2J\033[H");
                    resetAttrs();
                    break;
            }
            break;
        }
        case 'K': { // Effacement sur la ligne
            auto p = getParams();
            switch (p[0]) {
                case 0: emit("\033[0K"); break; // curseur -> fin ligne
                case 1: emit("\033[1K"); break; // début -> curseur
                case 2: emit("\033[2K"); break; // ligne entière
            }
            break;
        }
        default:
            // Séquence CSI non reconnue -> ignorée (§1.2.7)
            break;
    }
}

// ============================================================
//  Traitement SS2 (jeu G2 : accents et symboles)
// ============================================================
void ACIA6551::processSS2(uint8_t b) {
    state = State::NORMAL;
    emitAttrs();
    emit(g2ToUtf8(b));
}

// ============================================================
//  Traitement US (positionnement curseur)
//  Syntaxe : US 0x40+row 0x40+col  (bits utiles sur 6 bits)
// ============================================================
void ACIA6551::processUS(uint8_t b) {
    if (state == State::US1) {
        // Premier byte après US = rangée (b4..b0 = rangée, col 4-7 du tableau)
        if (b >= 0x40) {
            us_row = b & 0x3F;  // 6 bits utiles
            state = State::US2;
        } else {
            state = State::NORMAL;  // invalide
        }
    } else {
        // Deuxième byte = colonne
        state = State::NORMAL;
        if (b >= 0x40) {
            int col = b & 0x3F;
            // Rangée/colonne Videotex sont en base 0x40, 1-indexed
            // On convertit en coordonnées ANSI (1-indexed aussi)
            int row = (us_row > 0) ? us_row : 1;
            if (col == 0) col = 1;
            emit("\033[" + std::to_string(row) + ";" + std::to_string(col) + "H");
        }
        // Si invalide : US est ignoré
    }
}

// ============================================================
//  Conversion G1 (semi-graphique) -> UTF-8
// ============================================================
std::string ACIA6551::g1ToUtf8(uint8_t b) {
    // Les codes G1 vont de 0x20 à 0x7F
    // bit 6 = mode séparateur (disjoint), on l'ignore pour l'affichage
    // bits 5..0 = zones allumées
    if (b < 0x20) return " ";
    uint8_t bits = (b & 0x3F);  // 6 bits de zones (masque bit6 = séparateur)
    if (bits >= 64) bits = 63;
    return G1_UNICODE[bits];
}

// ============================================================
//  Conversion G2 (accents/symboles) -> UTF-8
//  Source : Schémas 2.8 (VGP5) du PDF STUM 1B
// ============================================================
std::string ACIA6551::g2ToUtf8(uint8_t b) {
    // Colonne 2 : symboles monétaires et flèches
    // Colonne 3 : fractions et flèches
    // Colonne 4 : diacritiques (accents sans avance)
    // Colonne 5 : exposants/...
    // Colonnes 6-7 : lettres spéciales
    switch (b) {
        // Col 2 (0x20-0x2F) : symboles spéciaux
        case 0x23: return "\xc2\xa3";     // £ Livre sterling
        case 0x24: return "$";
        case 0x26: return "#";
        case 0x27: return "\xc2\xa7";     // § paragraphe
        // Col 3 (0x30-0x3F) : fractions, flèches
        case 0x30: return "\xc2\xb0";     // ° degré
        case 0x31: return "\xc2\xb1";     // ± plus-moins
        case 0x38: return "\xc3\xb7";     // ÷ division
        case 0x3C: return "\xe2\x86\x90"; // ← flèche gauche
        case 0x3D: return "\xc2\xbc";     // ¼
        case 0x3E: return "\xe2\x86\x91"; // ↑ flèche haut
        case 0x3F: return "\xc2\xbd";     // ½
        // Col 4 (0x40-0x4F) : diacritiques (accent + lettre suivante)
        // En Videotex, l'accent ne provoque pas d'avance.
        // On les stocke et on les combine avec le char suivant.
        // Pour simplifier : on retourne juste l'accent combiné courant.
        case 0x41: return "\xcc\x80";     // ` accent grave (combining)
        case 0x42: return "\xcc\x81";     // ´ accent aigu (combining)
        case 0x43: return "\xcc\x82";     // ^ accent circonflexe
        case 0x44: return "\xcc\x83";     // ~ tilde
        case 0x48: return "\xcc\x88";     // ¨ tréma
        case 0x4B: return "\xcc\xa7";     // , cédille
        // Col 5 (0x50-0x5F) : autres diacritiques
        case 0x51: return "\xe2\x80\x98"; // ` guillemet
        case 0x52: return "\xe2\x80\x99"; // ' guillemet
        // Col 6 (0x60-0x6F) : ligatures et lettres spéciales
        case 0x6A: return "\xc5\x93";     // œ ligature oe
        case 0x6B: return "\xc3\x9f";     // ß eszett
        // Col 7 (0x70-0x7F)
        case 0x7A: return "\xc5\x92";     // Œ ligature OE
        // Défaut : caractère inconnu -> espace
        default:   return " ";
    }
}
