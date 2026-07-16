#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <thread>
#include <windows.h>
#include "Bus.h"
#include "CPU65C02.h"

namespace fs = std::filesystem;

// ============================================================
//  Active le mode ANSI (VT100) dans la console Windows 10+
//  Sans cela, les séquences \033[...m s'affichent en clair.
// ============================================================
static void enableAnsiConsole() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    // 1. Activer les séquences ANSI/VT100 (Windows 10+)
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode))
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // 2. UTF-8 pour les caractères semi-graphiques G1
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 3. Configurer le buffer PRINCIPAL (celui utilisé en mode rouleau,
    //    par ex. MS-BASIC) AVANT de basculer sur l'écran alternatif.
    //    Contrairement au mode page, ce buffer doit avoir une VRAIE
    //    marge de défilement (beaucoup plus de lignes que la fenêtre
    //    visible), pour que le retour à la ligne en bas d'écran fasse
    //    défiler naturellement le contenu, comme n'importe quel
    //    terminal classique. Un buffer de taille IDENTIQUE à la
    //    fenêtre (comme on le faisait avant) oblige la console à
    //    "décaler" tout le buffer à chaque ligne au lieu de simplement
    //    faire glisser la fenêtre dans un buffer plus grand — une
    //    opération plus coûteuse qui peut mal se comporter, en
    //    particulier sous Windows Terminal (ConPTY), et qui semble
    //    être la cause des soucis observés lors d'un collage en fin
    //    de page.
    {
        COORD mainBufSize = { 40, 5000 };          // 40 colonnes, grande marge de scroll
        SMALL_RECT mainWinRect = { 0, 0, 39, 24 }; // fenêtre visible : 40 x 25
        SetConsoleScreenBufferSize(hOut, mainBufSize);
        SetConsoleWindowInfo(hOut, TRUE, &mainWinRect);
    }

    // 4. Passer sur l'écran alternatif ANSI (buffer séparé, sans barre de scroll)
    //    Séquence ANSI standard : ESC [ ? 1049 h
    //    Cela crée un écran vierge et évite tout scroll parasite.
    std::cout << "\033[?1049h";
    std::cout.flush();

    // 5. Configurer le buffer ALTERNATIF (mode page, menu/WOZMON) à
    //    exactement 40 x 25 : ici on VEUT qu'il n'y ait aucune marge,
    //    puisque le mode page ne doit jamais défiler (comportement
    //    authentique du Minitel).
    {
        COORD altBufSize = { 40, 25 };
        SMALL_RECT altWinRect = { 0, 0, 39, 24 };
        SetConsoleWindowInfo(hOut, TRUE, &altWinRect);
        SetConsoleScreenBufferSize(hOut, altBufSize);
        SetConsoleWindowInfo(hOut, TRUE, &altWinRect);
    }

    // 5. Cacher le curseur au démarrage (le Minitel le gère lui-même via CON/COFF)
    std::cout << "\033[?25l";
    std::cout.flush();

    // 6. Configurer le mode d'ENTRÉE de la console pour ReadConsoleInput.
    //    Par défaut, une console Windows a ENABLE_LINE_INPUT (attend la
    //    touche Entrée avant de livrer quoi que ce soit) et
    //    ENABLE_ECHO_INPUT (Windows affiche lui-même ce qui est tapé).
    //    On désactive ces deux-là : on veut recevoir CHAQUE caractère
    //    immédiatement (comme le faisait _getch() avant), et c'est
    //    notre propre émulation Minitel qui se charge de l'affichage
    //    (sinon chaque caractère tapé ou collé apparaîtrait en double).
    //
    //    ENABLE_PROCESSED_INPUT est ÉGALEMENT désactivé : par défaut ce
    //    mode fait que Windows intercepte Ctrl+C AVANT même qu'il
    //    n'atteigne notre programme, pour déclencher l'arrêt du
    //    processus. En le désactivant, Ctrl+C n'est plus un signal
    //    système : il arrive comme un octet normal (0x03, ETX) dans le
    //    flux clavier, exactement comme n'importe quelle autre touche,
    //    et c'est donc la ROM du Memo-1 qui décide de ce qu'il faut en
    //    faire (interruption d'un programme BASIC en cours, comme sur
    //    un vrai terminal des années 70/80), pas Windows.
    //
    //    Pour quitter proprement l'émulateur, on utilise désormais
    //    Ctrl+Q (voir readKeyboardInput) ou simplement la fermeture de
    //    la fenêtre (toujours captée via SetConsoleCtrlHandler, qui
    //    n'est PAS affecté par ENABLE_PROCESSED_INPUT pour les
    //    événements de fermeture/déconnexion).
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        DWORD inMode = 0;
        if (GetConsoleMode(hIn, &inMode)) {
            inMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
            SetConsoleMode(hIn, inMode);
        }
    }
}

// Restaurer le terminal à la fermeture (fenêtre fermée, Ctrl+Q, etc.)
static void restoreConsole() {
    // Quitter l'écran alternatif -> revenir au terminal normal
    std::cout << "\033[?1049l";
    std::cout << "\033[?25h";  // remettre le curseur visible
    std::cout.flush();
}

// ------------------------------------------------------------
//  QUITTER proprement (Ctrl+Q) : restaure le terminal et sort.
//  Nécessaire depuis que Ctrl+C est laissé à la ROM du Memo-1
//  plutôt que d'être intercepté par Windows.
// ------------------------------------------------------------
static void handleQuit() {
    restoreConsole();
    std::exit(0);
}

// ============================================================
//  Chargement ROM interne
// ============================================================
bool loadROM(const std::string& filename, Bus& bus) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[ERREUR] Impossible d'ouvrir : " << filename << "\n";
        return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size == 32768) {
        std::cout << "[INFO] ROM 32Ko detectee. Chargement de la 2eme moitie...\n";
        file.seekg(16384);
        file.read(reinterpret_cast<char*>(bus.getROM().data()), 16384);
    } else if (size > 0 && size <= 16384) {
        std::cout << "[INFO] ROM " << size << " octets.\n";
        file.read(reinterpret_cast<char*>(bus.getROM().data()), size);
    } else {
        std::cerr << "[ERREUR] Taille inattendue : " << size << " octets.\n";
        return false;
    }
    std::cout << "[OK] ROM interne chargee.\n";
    return true;
}

// ============================================================
//  Dossier contenant l'exécutable (pour ROM, cartouches)
// ============================================================
std::string exeDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exe(path);
    size_t pos = exe.find_last_of("\\/");
    return (pos != std::string::npos) ? exe.substr(0, pos + 1) : "";
}

// ============================================================
//  Dossier "HDD" : disque dur virtuel du Memo-1, où SAVE écrit
//  et où LOAD va chercher les programmes .bas. Créé automatiquement
//  au premier lancement s'il n'existe pas encore.
// ============================================================
std::string hddDir() {
    std::string dir = exeDir() + "HDD\\";
    std::error_code ec;
    fs::create_directories(dir, ec);  // ne fait rien si déjà présent
    return dir;
}

// ============================================================
//  Recherche memo1_rom.bin dans le dossier de l'exe
// ============================================================
std::string findROM() {
    return exeDir() + "memo1_rom.bin";
}

// ============================================================
//  SAUVEGARDE / CHARGEMENT de programmes BASIC (.bas)
// ============================================================
//
//  Principe : on ne touche à rien dans la ROM. On pilote BASIC
//  depuis l'extérieur, exactement comme le ferait un utilisateur :
//    - SAVE  = on tape "LIST" à la place de l'utilisateur, on capture
//              tout ce que BASIC affiche en réponse (qui est déjà du
//              texte ASCII propre), et on écrit ça dans un fichier.
//    - LOAD  = on tape "NEW" puis on "colle" le contenu du fichier,
//              en réutilisant exactement le même mécanisme fiable
//              que pour un copier-coller manuel.
//
//  Raccourcis clavier : Ctrl+S (sauvegarder), Ctrl+O (charger).

// Exécute des cycles CPU en continu (avec tick/flush réguliers)
static void runCyclesBlocking(Bus& bus, CPU65C02& cpu, long long n) {
    for (long long i = 0; i < n; i++) {
        cpu.irq_line = bus.getACIA().hasInterrupt();
        cpu.clock();
        if (i % 1000 == 0) {
            bus.getVIA().tick(1000);
            bus.getACIA().tick(1000);
            bus.getACIA().flushDisplay();
        }
    }
    bus.getACIA().flushDisplay();
}

// Injecte une chaîne de caractères dans l'ACIA (comme un collage)
static void injectString(Bus& bus, const std::string& s) {
    for (char c : s) bus.getACIA().injectByte(static_cast<uint8_t>(c));
}

// Lit une ligne de texte tapée par l'utilisateur directement au
// clavier (PAS via l'ACIA/Minitel : une vraie petite invite de
// commande, pour choisir un nom de fichier). On bascule
// temporairement la console en mode ligne+écho classique, puis on
// restaure notre mode "brut" habituel juste après.
static std::string promptLine(const std::string& prompt) {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

    DWORD savedMode = 0;
    GetConsoleMode(hIn, &savedMode);
    // IMPORTANT : ENABLE_LINE_INPUT ne traite correctement le retour
    // chariot (Entrée), le retour arrière et le saut de ligne QUE si
    // ENABLE_PROCESSED_INPUT est ÉGALEMENT actif (comportement
    // documenté par Microsoft). Comme ce dernier est désactivé en
    // permanence ailleurs (pour laisser Ctrl+C atteindre la ROM), il
    // faut le réactiver ICI, le temps de cette invite, sinon la
    // touche Entrée n'est jamais reconnue et std::getline reste
    // bloqué indéfiniment.
    SetConsoleMode(hIn, savedMode | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    FlushConsoleInputBuffer(hIn);

    std::cout << "\n\033[0m" << prompt;
    std::cout.flush();

    std::string line;
    std::getline(std::cin, line);
    std::cin.clear();

    // Restaurer notre mode brut habituel (voir enableAnsiConsole) :
    // ENABLE_PROCESSED_INPUT redevient inactif, Ctrl+C repart vers la ROM.
    SetConsoleMode(hIn, savedMode);
    FlushConsoleInputBuffer(hIn);

    return line;
}

// Nettoie un nom de fichier fourni par l'utilisateur : retire tout
// séparateur de chemin (sécurité) et ajoute l'extension .bas si absente.
static std::string sanitizeBasName(std::string name) {
    // Retirer les caractères de chemin pour rester dans le dossier de l'exe
    name.erase(std::remove(name.begin(), name.end(), '/'), name.end());
    name.erase(std::remove(name.begin(), name.end(), '\\'), name.end());
    name.erase(std::remove(name.begin(), name.end(), ':'), name.end());
    // Retirer espaces en début/fin
    while (!name.empty() && name.front() == ' ') name.erase(name.begin());
    while (!name.empty() && name.back()  == ' ') name.pop_back();

    if (name.size() < 4 ||
        !(name.substr(name.size() - 4) == ".bas" || name.substr(name.size() - 4) == ".BAS")) {
        name += ".bas";
    }
    return name;
}

// ------------------------------------------------------------
//  SAVE : capture la sortie de LIST et l'écrit dans un fichier
// ------------------------------------------------------------
static void handleSave(Bus& bus, CPU65C02& cpu) {
    std::string filename = promptLine("[SAUVEGARDE] Nom du fichier (sans extension) : ");
    if (filename.empty()) {
        std::cout << "[ANNULE] Aucun nom fourni.\n";
        return;
    }
    filename = sanitizeBasName(filename);

    bus.getACIA().startCapture();
    injectString(bus, "LIST\r");

    // On attend que la capture se termine par "OK" suivi d'un retour
    // à la ligne (signe que BASIC a fini le LIST et repris la main),
    // avec un plafond de temps généreux pour les gros programmes.
    const long long CHUNK = 200000;
    const long long MAX_TOTAL = 600LL * 1000000LL; // ~10 min de cycles, tres large
    long long waited = 0;
    bool finished = false;
    while (waited < MAX_TOTAL) {
        runCyclesBlocking(bus, cpu, CHUNK);
        waited += CHUNK;
        const std::string& cap = bus.getACIA().captured();
        // La capture normalise chaque CR en un unique '\n' (voir
        // ACIA6551::processNormal, case 0x0D), donc le prompt "OK" de
        // retour de BASIC apparaît toujours comme la séquence exacte
        // "OK\n" (3 caractères) en fin de buffer capturé.
        if (cap.size() >= 3 && cap.compare(cap.size() - 3, 3, "OK\n") == 0) {
            finished = true;
            break;
        }
    }
    bus.getACIA().stopCapture();

    std::string captured = bus.getACIA().captured();
    if (!finished) {
        std::cout << "[ERREUR] La sauvegarde a expire (programme trop long ?).\n";
        return;
    }

    // Nettoyage du texte capturé :
    // 1) retirer l'écho de la commande "LIST" (première ligne)
    size_t firstNl = captured.find('\n');
    std::string body = (firstNl != std::string::npos) ? captured.substr(firstNl + 1) : captured;

    // 2) retirer les lignes vides en tête
    while (!body.empty() && body.front() == '\n') body.erase(body.begin());

    // 3) retirer le "OK" final (et la ligne vide qui le précède souvent)
    size_t okPos = body.rfind("OK\n");
    if (okPos != std::string::npos && okPos >= body.size() - 4) {
        body.erase(okPos);
    }
    while (!body.empty() && (body.back() == '\n' || body.back() == '\r')) body.pop_back();
    body += "\n";

    std::string path = hddDir() + filename;
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        std::cout << "[ERREUR] Impossible d'ecrire : " << path << "\n";
        return;
    }
    out << body;
    out.close();

    std::cout << "[OK] Programme sauvegarde -> HDD\\" << filename
               << " (" << body.size() << " octets)\n";
}

// ------------------------------------------------------------
//  LOAD : liste les .bas disponibles, puis colle le fichier choisi
// ------------------------------------------------------------
static void handleLoad(Bus& bus, CPU65C02& cpu) {
    std::vector<std::string> basFiles;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(hddDir(), ec)) {
        if (!entry.is_regular_file()) continue;
        std::string fn = entry.path().filename().string();
        std::string ext = entry.path().extension().string();
        for (auto& c : ext) c = static_cast<char>(tolower(c));
        if (ext == ".bas") basFiles.push_back(fn);
    }
    std::sort(basFiles.begin(), basFiles.end());

    std::cout << "\n\033[0m[CHARGEMENT] Fichiers .bas disponibles dans HDD\\ :\n";
    if (basFiles.empty()) {
        std::cout << "  (aucun fichier .bas dans le dossier HDD)\n";
    } else {
        for (size_t i = 0; i < basFiles.size(); i++) {
            std::cout << "  " << (i + 1) << ". " << basFiles[i] << "\n";
        }
    }

    std::string choice = promptLine("Numero ou nom de fichier (Entree pour annuler) : ");
    if (choice.empty()) {
        std::cout << "[ANNULE]\n";
        return;
    }

    std::string filename;
    bool isNumber = !choice.empty() &&
        std::all_of(choice.begin(), choice.end(), [](char c){ return isdigit((unsigned char)c); });
    if (isNumber) {
        size_t idx = static_cast<size_t>(std::stoi(choice)) - 1;
        if (idx >= basFiles.size()) {
            std::cout << "[ERREUR] Numero invalide.\n";
            return;
        }
        filename = basFiles[idx];
    } else {
        filename = sanitizeBasName(choice);
    }

    std::string path = hddDir() + filename;
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        std::cout << "[ERREUR] Fichier introuvable : " << path << "\n";
        return;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();

    // Normaliser les fins de ligne vers '\r' (attendu par BASIC),
    // quelle que soit leur forme d'origine (\n, \r\n, \r).
    std::string normalized;
    normalized.reserve(content.size());
    for (size_t i = 0; i < content.size(); i++) {
        char c = content[i];
        if (c == '\r') {
            normalized += '\r';
            if (i + 1 < content.size() && content[i + 1] == '\n') i++;
        } else if (c == '\n') {
            normalized += '\r';
        } else {
            normalized += c;
        }
    }
    if (normalized.empty() || normalized.back() != '\r') normalized += '\r';

    std::cout << "[CHARGEMENT] HDD\\" << filename << " (" << normalized.size()
               << " octets) -> injection en cours...\n";

    // On efface le programme courant avant de charger le nouveau,
    // pour ne pas mélanger d'anciennes lignes avec les nouvelles.
    injectString(bus, "NEW\r");
    runCyclesBlocking(bus, cpu, 2000000);

    injectString(bus, normalized);

    std::cout << "[OK] Fichier injecte. Il sera tape au rythme normal.\n";
}

// ============================================================
//  Lecture clavier via l'API Win32 native (ReadConsoleInput)
// ============================================================
//  _kbhit()/_getch() (conio.h) sont d'anciennes fonctions du runtime C
//  héritées de MS-DOS. Elles passent par une couche d'abstraction du
//  CRT qui n'offre AUCUNE garantie documentée de fiabilité en cas de
//  rafale massive de caractères (comme un collage de plusieurs milliers
//  de caractères d'un coup) : selon la charge système, des caractères
//  peuvent être perdus ou l'ordre peut être altéré.
//
//  ReadConsoleInput est l'API Win32 native et documentée : elle lit
//  directement, dans l'ordre strict FIFO, la file d'événements interne
//  de la console. C'est le mécanisme par lequel TOUT texte collé dans
//  une console Windows transite (qu'il s'agisse de conhost.exe ou de
//  Windows Terminal/ConPTY), donc c'est le chemin le plus fiable pour
//  ne perdre aucun caractère lors d'un copier-coller volumineux.
static void readKeyboardInput(Bus& bus, CPU65C02& cpu) {
    static HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE) return;

    static INPUT_RECORD buffer[2048];

    // On boucle tant qu'il reste des événements en attente, pour ne
    // jamais laisser un gros collage déborder d'un seul appel.
    for (;;) {
        DWORD pending = 0;
        if (!GetNumberOfConsoleInputEvents(hIn, &pending) || pending == 0) break;

        DWORD toRead = (pending > 2048) ? 2048 : pending;
        DWORD nbRead = 0;
        if (!ReadConsoleInputA(hIn, buffer, toRead, &nbRead) || nbRead == 0) break;

        for (DWORD i = 0; i < nbRead; i++) {
            if (buffer[i].EventType != KEY_EVENT) continue;
            const KEY_EVENT_RECORD& kev = buffer[i].Event.KeyEvent;
            if (!kev.bKeyDown) continue;  // ignorer le relâchement de touche

            // wRepeatCount reflète le nombre de fois où la touche a
            // généré ce même caractère (touche maintenue enfoncée) ;
            // on les injecte toutes pour ne rien perdre.
            for (WORD r = 0; r < kev.wRepeatCount; r++) {
                char c = kev.uChar.AsciiChar;

                // Raccourcis SAVE (Ctrl+S = 0x13), LOAD (Ctrl+O = 0x0F)
                // et QUITTER (Ctrl+Q = 0x11). Interceptés ICI, avant
                // d'atteindre l'ACIA : ce sont des raccourcis propres à
                // l'émulateur, jamais envoyés au Memo-1 lui-même.
                //
                // Ctrl+C n'est PAS intercepté ici : il est volontairement
                // laissé passer tel quel (comme un octet 0x03 normal)
                // pour que ce soit la ROM du Memo-1 qui décide de son
                // comportement (interruption d'un programme BASIC en
                // cours, par exemple), au lieu que Windows ne ferme le
                // programme à sa place.
                if (c == 0x13) { handleSave(bus, cpu); continue; }
                if (c == 0x0F) { handleLoad(bus, cpu); continue; }
                if (c == 0x11) { handleQuit(); continue; }

                if (c != 0) {
                    bus.getACIA().injectByte(static_cast<uint8_t>(c));
                } else {
                    // Touche spéciale sans caractère ASCII direct (flèches)
                    switch (kev.wVirtualKeyCode) {
                        case VK_UP:    bus.getACIA().injectByte(0x1B); bus.getACIA().injectByte(0x0B); break;
                        case VK_DOWN:  bus.getACIA().injectByte(0x1B); bus.getACIA().injectByte(0x0A); break;
                        case VK_LEFT:  bus.getACIA().injectByte(0x1B); bus.getACIA().injectByte(0x08); break;
                        case VK_RIGHT: bus.getACIA().injectByte(0x1B); bus.getACIA().injectByte(0x09); break;
                        default: break;
                    }
                }
            }
        }

        // Si on a lu moins que ce qui était annoncé pending, ou moins
        // que la capacité du buffer, il est probable qu'il ne reste
        // plus rien -> la boucle suivante le vérifiera via pending==0.
    }
}

// ============================================================
//  Boucle d'émulation
// ============================================================
void runEmulator(Bus& bus, CPU65C02& cpu) {
    const int  CYCLES_PAR_MS = 1000;
    const auto DUREE_MS      = std::chrono::microseconds(1000);

    while (true) {
        auto t0 = std::chrono::high_resolution_clock::now();

        // Lecture clavier non-bloquante (API Win32 native, voir plus haut)
        readKeyboardInput(bus, cpu);

        // Exécution CPU
        for (int i = 0; i < CYCLES_PAR_MS; ++i) {
            cpu.irq_line = bus.getACIA().hasInterrupt();
            cpu.clock();
        }

        // Affichage
        bus.getACIA().flushDisplay();

        // Tick VIA
        bus.getVIA().tick(CYCLES_PAR_MS);
        bus.getACIA().tick(CYCLES_PAR_MS);

        // Synchronisation
        auto elapsed = std::chrono::high_resolution_clock::now() - t0;
        if (elapsed < DUREE_MS)
            std::this_thread::sleep_for(DUREE_MS - elapsed);
    }
}

// ============================================================
//  Point d'entrée
// ============================================================
int main(int argc, char* argv[]) {
    enableAnsiConsole();
    SetConsoleTitleA("MEMO-1 Emulator - Terminal Minitel");

    // Gestionnaire d'événements de fermeture (croix de la fenêtre,
    // déconnexion, arrêt du système, etc.) : restaure le terminal avant
    // de quitter. Ctrl+C ne déclenche PLUS cet événement (voir
    // enableAnsiConsole : ENABLE_PROCESSED_INPUT est désactivé), il est
    // désormais transmis tel quel à la ROM du Memo-1.
    SetConsoleCtrlHandler([](DWORD) -> BOOL {
        restoreConsole();
        return FALSE;  // laisser Windows gérer la terminaison
    }, TRUE);

    std::cout << "\033[0m";  // reset attributs ANSI
    std::cout << "========================================\n";
    std::cout << "  MEMO-1 Emulator (W65C02 @ 1MHz)\n";
    std::cout << "  Terminal Minitel Emule\n";
    std::cout << "========================================\n\n";
    std::cout.flush();

    Bus      bus;
    CPU65C02 cpu;
    cpu.connectBus(&bus);

    // Chargement ROM interne
    std::string romPath = findROM();
    if (!loadROM(romPath, bus)) {
        if (!loadROM("memo1_rom.bin", bus)) {
            std::cerr << "\n[ERREUR] memo1_rom.bin introuvable.\n";
            std::cerr << "Placez memo1_rom.bin dans le meme dossier que MEMU1.exe\n";
            std::cerr << "Appuyez sur Entree...";
            std::cin.get();
            return 1;
        }
    }

    // Chargement cartouche (glisser-déposer)
    if (argc >= 2) {
        std::string cartPath(argv[1]);
        bool isBin = cartPath.size() >= 4;
        if (isBin) {
            std::string ext = cartPath.substr(cartPath.size() - 4);
            for (auto& c : ext) c = static_cast<char>(tolower(c));
            isBin = (ext == ".bin");
        }
        if (!isBin) {
            std::cerr << "[AVERTISSEMENT] Pas un .bin : " << cartPath << "\n\n";
        } else if (!bus.loadCartridge(cartPath)) {
            std::cerr << "[AVERTISSEMENT] Impossible de charger : " << cartPath
                      << "\n  (8 Ko max)\n\n";
        } else {
            std::cout << "[OK] Cartouche : \"" << bus.cartridgeName() << "\"\n\n";
        }
    }

    if (bus.hasCartridge()) {
        std::cout << "[MEMO-1] Cartouche connectee -> option '4' dans le menu.\n";
    } else {
        std::cout << "[MEMO-1] Pas de cartouche. Glissez un .bin sur MEMU1.exe\n";
    }

    // S'assurer que le dossier HDD existe dès le démarrage (et pas
    // seulement au premier SAVE/LOAD), pour que l'utilisateur sache
    // tout de suite où ses programmes iront.
    std::string hdd = hddDir();
    std::cout << "[MEMO-1] Disque HDD pret : " << hdd << "\n";
    std::cout << "[MEMO-1] Ctrl+S = sauvegarder, Ctrl+O = charger un .bas, Ctrl+Q = quitter\n";

    std::cout << "[SYSTEME] Boucle active. (Ctrl+Q ou fermer la fenetre pour quitter)\n\n";
    std::cout.flush();

    cpu.reset();
    runEmulator(bus, cpu);
    return 0;
}
