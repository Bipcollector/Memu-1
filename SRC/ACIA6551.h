#pragma once
#include <cstdint>
#include <queue>
#include <string>

class ACIA6551 {
public:
    ACIA6551();
    ~ACIA6551() = default;

    uint8_t read(uint8_t reg);
    void write(uint8_t reg, uint8_t data);
    void pollKeyboard();

    [[nodiscard]] bool hasInterrupt() const {
        return (status & RDRF) && !(command & 0x02);
    }

    // Injecte un byte reçu du clavier hôte (appelé par main.cpp).
    // Le byte n'est PAS rendu disponible instantanément : il est mis en
    // attente dans rx_pending et ne devient visible pour le CPU (RDRF=1)
    // qu'après un délai simulant le débit série réel (voir tick()).
    // Cela évite de submerger l'interpréteur BASIC avec un débit
    // "infini" lors d'un copier-coller volumineux (voir tick()).
    void injectByte(uint8_t b) {
        rx_pending.push(b);
    }

    // Fait avancer l'horloge de réception série. À appeler à chaque
    // tranche de cycles CPU (comme VIA::tick()). Libère un nouveau
    // caractère du buffer d'attente vers le registre RX toutes les
    // RX_CYCLES_PER_BYTE cycles, simulant un débit série fixe.
    void tick(int elapsed_cycles) {
        if (rx_delay_counter > 0) {
            rx_delay_counter -= elapsed_cycles;
        }
        if (rx_delay_counter <= 0 && !(status & RDRF) && !rx_pending.empty()) {
            keyboard_buffer.push(rx_pending.front());
            rx_pending.pop();
            status |= RDRF;
            rx_delay_counter = RX_CYCLES_PER_BYTE;
        }
    }

    // Vide le buffer d'affichage vers la console Windows
    void flushDisplay();

    // ---- Capture de texte pour la fonction SAVE ----
    // Pendant une capture, chaque caractère "logique" imprimable (G0)
    // et chaque retour à la ligne (CR) envoyés par la ROM sont copiés
    // tels quels (SANS les codes couleur ANSI ni les séquences Videotex)
    // dans un buffer séparé. Utilisé pour extraire proprement le texte
    // produit par la commande LIST de BASIC, afin de l'écrire dans un
    // fichier .bas lisible.
    void startCapture() { capturing = true; capture_buf.clear(); }
    void stopCapture()  { capturing = false; }
    [[nodiscard]] const std::string& captured() const { return capture_buf; }
    [[nodiscard]] bool isCapturing() const { return capturing; }

private:
    // ---- Registres ACIA ----
    uint8_t rx_data = 0x00;
    uint8_t tx_data = 0x00;
    uint8_t status  = 0x00;
    uint8_t command = 0x00;
    uint8_t control = 0x00;
    std::queue<uint8_t> keyboard_buffer;

    // File d'attente "brute" pour les octets injectés par main.cpp
    // (frappe clavier ou copier-coller). Ils passent dans keyboard_buffer
    // un par un, au rythme simulé par tick()/RX_CYCLES_PER_BYTE, plutôt
    // que d'être tous rendus visibles au CPU en même temps.
    std::queue<uint8_t> rx_pending;
    int rx_delay_counter = 0;

    // Débit de réception simulé.
    //
    // IMPORTANT : ce délai ne sert pas seulement à "laisser le temps"
    // à BASIC d'échoïr un caractère. Quand BASIC insère une nouvelle
    // ligne dans son programme (à la réception du CR de fin de ligne),
    // il doit DÉCALER EN MÉMOIRE toutes les lignes de numéro supérieur
    // pour garder le programme trié. Ce décalage devient de plus en
    // plus coûteux (en cycles CPU) à mesure que le programme grossit.
    // Si le caractère suivant arrive trop tôt, l'IRQ clavier interrompt
    // BASIC EN PLEIN MILIEU de ce décalage mémoire, corrompant ses
    // pointeurs internes -> lignes perdues, "?SYNTAX ERROR" aléatoires.
    //
    // Cette valeur a été validée empiriquement sur des programmes
    // jusqu'à 30 Ko (442 lignes) sans AUCUNE perte ni corruption.
    // Elle correspond à un débit d'environ 166 octets/seconde simulés
    // à 1 MHz — plus lent qu'un Minitel réel (1200 bauds ≈ 120 car/s)
    // mais avec une marge de sécurité confortable pour les décalages
    // mémoire coûteux sur les gros programmes.
    //
    // Ordres de grandeur du temps de collage résultant :
    //   1 Ko  -> ~6 secondes
    //   10 Ko -> ~1 minute
    //   30 Ko -> ~3 minutes (programme proche du maximum de RAM)
    // C'est plus lent qu'on pourrait le souhaiter, mais c'est le prix
    // de la fiabilité : au-dessous de cette valeur, des programmes de
    // taille moyenne à grande perdent des lignes de façon aléatoire.
    static constexpr int RX_CYCLES_PER_BYTE = 10000;

    enum StatusFlags : uint8_t {
        PE   = 0x01, FE   = 0x02, OVRN = 0x04,
        RDRF = 0x08, TDRE = 0x10, DCD  = 0x20,
        CTS  = 0x40, IRQ  = 0x80
    };

    // ---- Buffer de sortie (accumulé, vidé par flushDisplay) ----
    std::string display_buf;

    // ---- Capture de texte pour SAVE (voir startCapture/stopCapture) ----
    bool capturing = false;
    std::string capture_buf;

    // ---- État de l'interpréteur Videotex ----

    // Attributs courants
    uint8_t fg_color  = 7;  // 0-7 (noir..blanc)
    uint8_t bg_color  = 0;
    bool    inverse   = false;
    bool    underline = false;

    // Mode d'affichage courant : false = mode page (défaut Minitel),
    // true = mode rouleau (défilement, activé par PRO2 START ROULEAU,
    // désactivé par PRO2 STOP ROULEAU). §4.2.1 du PDF STUM 1B.
    bool    mode_rouleau = false;

    // Cache des attributs RÉELLEMENT émis au terminal (pour éviter de
    // renvoyer une séquence ANSI à chaque caractère si rien n'a changé)
    uint8_t emitted_fg  = 255; // valeur impossible -> force le 1er emitAttrs()
    uint8_t emitted_bg  = 255;
    bool    emitted_inv = false;
    bool    emitted_und = false;
    bool    attrs_emitted_once = false;

    // Jeu de caractères actif
    enum class CharSet { G0, G1, G2_NEXT };
    CharSet charset = CharSet::G0;

    // Automate d'état principal
    enum class State {
        NORMAL,      // affichage standard G0
        ESC,         // reçu ESC (0x1B)
        CSI,         // reçu ESC 0x5B -> séquence CSI ISO 6429
        SS2,         // reçu SS2 (0x19) -> prochain char = G2
        US1,         // reçu US (0x1F)  -> prochain char = rangée
        US2,         // reçu US + rangée -> prochain char = colonne
        ESC_PRO,     // séquence PRO1/PRO2/PRO3 : avaler N bytes
    };
    State   state = State::NORMAL;
    int     pro_skip = 0;   // bytes restants à avaler pour PRO1/PRO2/PRO3
    uint8_t pro_buf[3] = {};// bytes accumulés dans la séquence PRO
    int     pro_idx  = 0;   // index courant dans pro_buf
    int     pro_total= 0;   // longueur totale de la séquence PRO

    // Buffer de paramètres CSI
    std::string csi_params;   // digits + ';' accumulés
    uint8_t     us_row = 0;   // rangée lue après US

    // ---- Méthodes internes ----
    void processNormal(uint8_t b);
    void processEsc(uint8_t b);
    void processCsi(uint8_t b);
    void processSS2(uint8_t b);
    void processUS(uint8_t b);

    // Émet la séquence ANSI courante (couleurs + attributs)
    void emitAttrs();
    // Réinitialise les attributs à blanc sur noir
    void resetAttrs();

    // Convertit un caractère G1 (semi-graphique) en UTF-8 via blocs Unicode
    std::string g1ToUtf8(uint8_t b);
    // Convertit un caractère G2 (accent/symbole) en UTF-8
    std::string g2ToUtf8(uint8_t b);

    // Émet directement dans display_buf
    void emit(const std::string& s) { display_buf += s; }
    void emit(char c)               { display_buf += c; }
};
