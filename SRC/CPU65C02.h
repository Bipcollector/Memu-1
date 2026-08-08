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
#include "Bus.h"



class CPU65C02 {
public:

    

    CPU65C02() {
        buildLookupTable();
    }
    ~CPU65C02() = default;

    void connectBus(Bus* bus) { this->bus = bus; }
    void reset();
    void clock();

    bool irq_line = false;
    bool nmi_line = false;

private:
    uint8_t a = 0x00; uint8_t x = 0x00; uint8_t y = 0x00; uint8_t sp = 0x00;
    uint16_t pc = 0x0000; uint8_t status = 0x00;
    uint8_t cycles = 0;
    Bus* bus = nullptr;

    enum Flag : uint8_t { C = (1 << 0), Z = (1 << 1), I = (1 << 2), D = (1 << 3), B = (1 << 4), U = (1 << 5), V = (1 << 6), N = (1 << 7) };

    // --- Fonctions de base ---
    void setFlag(Flag f, bool v) { if (v) status |= f; else status &= ~f; }
    [[nodiscard]] bool getFlag(Flag f) const { return (status & f) > 0; }
    
    [[nodiscard]] uint8_t read(uint16_t addr) { return bus->read(addr); }
    void write(uint16_t addr, uint8_t data) { bus->write(addr, data); }
    [[nodiscard]] uint8_t fetch() { return read(pc++); }
    void push(uint8_t data) { write(0x0100 + sp, data); sp--; }
    uint8_t pull() { sp++; return read(0x0100 + sp); }

    // --- Modes d'adressage ---
    [[nodiscard]] uint16_t addrIMP() { return 0x0000; }
    [[nodiscard]] uint16_t addrACC() { return 0xFFFF; } // Valeur sentinelle pour l'Accumulateur
    [[nodiscard]] uint16_t addrIMM() { return pc++; }
    [[nodiscard]] uint16_t addrZPG() { return fetch(); }
    [[nodiscard]] uint16_t addrZPX() { return (fetch() + x) & 0x00FF; }
    [[nodiscard]] uint16_t addrZPY() { return (fetch() + y) & 0x00FF; }
    [[nodiscard]] uint16_t addrABS() { uint16_t lo = fetch(); return lo | (fetch() << 8); }
    [[nodiscard]] uint16_t addrABX() { uint16_t lo = fetch(); return ((lo | (fetch() << 8)) + x) & 0xFFFF; }
    [[nodiscard]] uint16_t addrABY() { uint16_t lo = fetch(); return ((lo | (fetch() << 8)) + y) & 0xFFFF; }
    [[nodiscard]] uint16_t addrIND() {
        uint16_t ptr_lo = fetch(); uint16_t ptr_hi = fetch(); uint16_t ptr = (ptr_hi << 8) | ptr_lo;
        if (ptr_lo == 0x00FF) return read(ptr) | (read(ptr & 0xFF00) << 8); // Fix 65C02
        else return read(ptr) | (read(ptr + 1) << 8);
    }
    [[nodiscard]] uint16_t addrIZX() { uint16_t base = (fetch() + x) & 0x00FF; return read(base) | (read((base + 1) & 0x00FF) << 8); }
    [[nodiscard]] uint16_t addrIZY() { uint16_t base = fetch(); uint16_t lo = read(base); uint16_t hi = read((base + 1) & 0x00FF); return ((hi << 8) | lo) + y; }
    // 65C02 : Zero Page Indirect sans index - (zp)
    [[nodiscard]] uint16_t addrIZP() { uint16_t base = fetch(); return read(base) | (read((base + 1) & 0x00FF) << 8); }
    // 65C02 : JMP (abs,X)
    [[nodiscard]] uint16_t addrABX_IND() { uint16_t lo = fetch(); uint16_t ptr = (lo | (fetch() << 8)) + x; return read(ptr) | (read(ptr + 1) << 8); }
    [[nodiscard]] uint16_t addrREL() { int8_t offset = (int8_t)fetch(); return pc + offset; }

    // --- Opérations ALU ---
    void opADC(uint16_t addr) {
        uint8_t value = read(addr);
        if (getFlag(Flag::D)) {
            uint8_t al = (a & 0x0F) + (value & 0x0F) + getFlag(Flag::C); uint8_t ah = (a >> 4) + (value >> 4) + (al > 0x0F ? 1 : 0);
            setFlag(Flag::C, ah > 0x0F); setFlag(Flag::Z, ((ah << 4) | (al & 0x0F)) == 0); setFlag(Flag::N, ah & 0x08); setFlag(Flag::V, (~(a ^ value) & (a ^ (ah << 4))) & 0x80); a = (ah << 4) | (al & 0x0F);
        } else {
            uint16_t sum = a + value + getFlag(Flag::C); setFlag(Flag::C, sum > 0xFF); setFlag(Flag::Z, (sum & 0xFF) == 0); setFlag(Flag::V, (~(a ^ value) & (a ^ sum)) & 0x80); setFlag(Flag::N, sum & 0x80); a = sum & 0xFF;
        }
    }
    void opSBC(uint16_t addr) {
        // SBC : on lit la valeur à l'adresse, on la complète (XOR 0xFF),
        // puis on délègue à ADC. NE PAS XOR l'adresse elle-même !
        uint8_t val = read(addr);
        uint16_t tmp = addr; // on "triche" : on écrit la valeur inversée en RAM temp
        // En fait la bonne façon : dupliquer la logique ADC avec val^0xFF
        uint16_t sum = a + (uint8_t)(val ^ 0xFF) + getFlag(Flag::C);
        setFlag(Flag::C, sum > 0xFF);
        setFlag(Flag::Z, (sum & 0xFF) == 0);
        setFlag(Flag::V, (~(a ^ (uint8_t)(val ^ 0xFF)) & (a ^ sum)) & 0x80);
        setFlag(Flag::N, sum & 0x80);
        a = sum & 0xFF;
    }
    void opAND(uint16_t addr) { a &= read(addr); setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
    void opORA(uint16_t addr) { a |= read(addr); setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
    void opEOR(uint16_t addr) { a ^= read(addr); setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
    void opCMP(uint8_t reg, uint16_t addr) { uint8_t val = read(addr); setFlag(Flag::C, reg >= val); setFlag(Flag::Z, reg == val); setFlag(Flag::N, (reg - val) & 0x80); }
    void opBranch(bool condition) {
        // BUG CRITIQUE CORRIGÉ : l'octet d'offset du branchement DOIT être
        // consommé par fetch() dans tous les cas, que la condition soit
        // vraie ou fausse. Avant ce fix, addrREL() (qui appelle fetch())
        // n'était invoqué que si condition==true, donc une branche NON
        // prise laissait le PC pointer sur l'octet d'offset au lieu de
        // l'instruction suivante. Le CPU exécutait alors cet octet comme
        // un nouvel opcode, dérivant silencieusement de tout le programme.
        uint16_t addr = addrREL(); // consomme TOUJOURS l'offset (fetch obligatoire)
        if (condition) {
            cycles++;
            if ((addr & 0xFF00) != (pc & 0xFF00)) cycles++;
            pc = addr;
        }
    }

    void opASL(uint16_t addr) {
        if (addr == 0xFFFF) { setFlag(Flag::C, a & 0x80); a <<= 1; setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
        else { uint8_t val = read(addr); setFlag(Flag::C, val & 0x80); val <<= 1; setFlag(Flag::Z, val == 0); setFlag(Flag::N, val & 0x80); write(addr, val); }
    }
    void opLSR(uint16_t addr) {
        if (addr == 0xFFFF) { setFlag(Flag::C, a & 0x01); a >>= 1; setFlag(Flag::Z, a == 0); setFlag(Flag::N, false); }
        else { uint8_t val = read(addr); setFlag(Flag::C, val & 0x01); val >>= 1; setFlag(Flag::Z, val == 0); setFlag(Flag::N, false); write(addr, val); }
    }
    void opROL(uint16_t addr) {
        if (addr == 0xFFFF) { bool old_c = getFlag(Flag::C); setFlag(Flag::C, a & 0x80); a = (a << 1) | old_c; setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
        else { uint8_t val = read(addr); bool old_c = getFlag(Flag::C); setFlag(Flag::C, val & 0x80); val = (val << 1) | old_c; setFlag(Flag::Z, val == 0); setFlag(Flag::N, val & 0x80); write(addr, val); }
    }
    void opROR(uint16_t addr) {
        if (addr == 0xFFFF) { bool old_c = getFlag(Flag::C); setFlag(Flag::C, a & 0x01); a = (a >> 1) | (old_c << 7); setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
        else { uint8_t val = read(addr); bool old_c = getFlag(Flag::C); setFlag(Flag::C, val & 0x01); val = (val >> 1) | (old_c << 7); setFlag(Flag::Z, val == 0); setFlag(Flag::N, val & 0x80); write(addr, val); }
    }
    void opINC(uint16_t addr) { uint8_t val = read(addr) + 1; setFlag(Flag::Z, val == 0); setFlag(Flag::N, val & 0x80); write(addr, val); }
    void opDEC(uint16_t addr) { uint8_t val = read(addr) - 1; setFlag(Flag::Z, val == 0); setFlag(Flag::N, val & 0x80); write(addr, val); }

    // --- Templates d'Instructions (Haute Performance) ---
    template<uint16_t (CPU65C02::*AM)()> void LDA_Impl() { uint16_t addr = (this->*AM)(); a = read(addr); setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
    template<uint16_t (CPU65C02::*AM)()> void LDX_Impl() { uint16_t addr = (this->*AM)(); x = read(addr); setFlag(Flag::Z, x == 0); setFlag(Flag::N, x & 0x80); }
    template<uint16_t (CPU65C02::*AM)()> void LDY_Impl() { uint16_t addr = (this->*AM)(); y = read(addr); setFlag(Flag::Z, y == 0); setFlag(Flag::N, y & 0x80); }
    
    template<uint16_t (CPU65C02::*AM)()> void STA_Impl() { write((this->*AM)(), a); }
    template<uint16_t (CPU65C02::*AM)()> void STX_Impl() { write((this->*AM)(), x); }
    template<uint16_t (CPU65C02::*AM)()> void STY_Impl() { write((this->*AM)(), y); }

    template<uint16_t (CPU65C02::*AM)()> void ADC_Impl() { opADC((this->*AM)()); }
    template<uint16_t (CPU65C02::*AM)()> void SBC_Impl() { opSBC((this->*AM)()); }
    template<uint16_t (CPU65C02::*AM)()> void AND_Impl() { opAND((this->*AM)()); }
    template<uint16_t (CPU65C02::*AM)()> void ORA_Impl() { opORA((this->*AM)()); }
    template<uint16_t (CPU65C02::*AM)()> void EOR_Impl() { opEOR((this->*AM)()); }
    
    template<uint16_t (CPU65C02::*AM)()> void CMP_Impl() { opCMP(a, (this->*AM)()); }
    template<uint16_t (CPU65C02::*AM)()> void CPX_Impl() { opCMP(x, (this->*AM)()); }
    template<uint16_t (CPU65C02::*AM)()> void CPY_Impl() { opCMP(y, (this->*AM)()); }

    template<uint16_t (CPU65C02::*AM)()> void INC_Impl() { opINC((this->*AM)()); }
    template<uint16_t (CPU65C02::*AM)()> void DEC_Impl() { opDEC((this->*AM)()); }
    template<uint16_t (CPU65C02::*AM)()> void ASL_Impl() { opASL((this->*AM)()); }
    template<uint16_t (CPU65C02::*AM)()> void LSR_Impl() { opLSR((this->*AM)()); }
    template<uint16_t (CPU65C02::*AM)()> void ROL_Impl() { opROL((this->*AM)()); }
    template<uint16_t (CPU65C02::*AM)()> void ROR_Impl() { opROR((this->*AM)()); }

    // --- Fonctions Instructions Standard ---
    void NOP() {}
    void TAX() { x = a; setFlag(Flag::Z, x == 0); setFlag(Flag::N, x & 0x80); }
    void TAY() { y = a; setFlag(Flag::Z, y == 0); setFlag(Flag::N, y & 0x80); }
    void TXA() { a = x; setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
    void TYA() { a = y; setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
    void TSX() { x = sp; setFlag(Flag::Z, x == 0); setFlag(Flag::N, x & 0x80); }
    void TXS() { sp = x; }
    void PHA() { push(a); } void PHP() { push(status | Flag::B | Flag::U); }
    void PLA() { a = pull(); setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
    void PLP() { status = (pull() & ~Flag::B) | Flag::U; }
    void PHX() { push(x); } void PHY() { push(y); }
    void PLX() { x = pull(); setFlag(Flag::Z, x == 0); setFlag(Flag::N, x & 0x80); }
    void PLY() { y = pull(); setFlag(Flag::Z, y == 0); setFlag(Flag::N, y & 0x80); }
    void INX() { x++; setFlag(Flag::Z, x == 0); setFlag(Flag::N, x & 0x80); }
    void INY() { y++; setFlag(Flag::Z, y == 0); setFlag(Flag::N, y & 0x80); }
    void DEX() { x--; setFlag(Flag::Z, x == 0); setFlag(Flag::N, x & 0x80); }
    void DEY() { y--; setFlag(Flag::Z, y == 0); setFlag(Flag::N, y & 0x80); }
    
    void JMP_ABS() { pc = addrABS(); }
    void JMP_IND() { pc = addrIND(); }
    void JSR() {
        // BUG CRITIQUE CORRIGÉ : l'adresse de retour doit être calculée
        // APRÈS avoir consommé les 2 bytes d'opérande de l'adresse cible,
        // pas avant. Sinon RTS revient 2 bytes trop tôt et le CPU
        // réexécute un octet d'adresse comme s'il s'agissait d'un opcode,
        // ce qui corrompt durablement l'exécution.
        uint16_t target = addrABS();      // consomme les 2 bytes, pc avance de 2
        uint16_t ret = pc - 1;            // pc pointe maintenant juste après JSR+2bytes
        push(ret >> 8); push(ret & 0xFF);
        pc = target;
    }
    void RTS() { uint16_t lo = pull(); pc = (pull() << 8) | lo; pc++; }
    void RTI() { status = (pull() & ~Flag::B) | Flag::U; uint16_t lo = pull(); pc = (pull() << 8) | lo; }
    
    void BCC() { opBranch(!getFlag(Flag::C)); } void BCS() { opBranch(getFlag(Flag::C)); }
    void BEQ() { opBranch(getFlag(Flag::Z)); } void BNE() { opBranch(!getFlag(Flag::Z)); }
    void BMI() { opBranch(getFlag(Flag::N)); } void BPL() { opBranch(!getFlag(Flag::N)); }
    void BVS() { opBranch(getFlag(Flag::V)); } void BVC() { opBranch(!getFlag(Flag::V)); }
    void BRA() { opBranch(true); }
    
    void CLC() { setFlag(Flag::C, false); } void SEC() { setFlag(Flag::C, true); }
    void CLD() { setFlag(Flag::D, false); } void SED() { setFlag(Flag::D, true); }
    void CLI() { setFlag(Flag::I, false); } void SEI() { setFlag(Flag::I, true); }
    void CLV() { setFlag(Flag::V, false); }
    void BRK() { pc++; push(pc >> 8); push(pc & 0xFF); push(status | Flag::B | Flag::U); setFlag(Flag::I, true); pc = read(0xFFFE) | (read(0xFFFF) << 8); }

    void STZ() { 
        uint8_t op = read(pc-1); 
        if(op == 0x64) write(addrZPG(), 0x00); else if(op == 0x74) write(addrZPX(), 0x00); 
        else if(op == 0x9C) write(addrABS(), 0x00); else if(op == 0x9E) write(addrABX(), 0x00); 
        setFlag(Flag::Z, true); 
    }
    void TSB() { uint8_t op = read(pc-1); uint16_t addr = (op == 0x04) ? addrZPG() : addrABS(); uint8_t val = read(addr); setFlag(Flag::Z, (a & val) == 0); write(addr, val | a); }
    void TRB() { uint8_t op = read(pc-1); uint16_t addr = (op == 0x14) ? addrZPG() : addrABS(); uint8_t val = read(addr); setFlag(Flag::Z, (a & val) == 0); write(addr, val & ~a); }
    
    void BIT_ZPG() { uint8_t val = read(addrZPG()); setFlag(Flag::Z, (a & val) == 0); setFlag(Flag::N, val & 0x80); setFlag(Flag::V, val & 0x40); }
    void BIT_ABS() { uint8_t val = read(addrABS()); setFlag(Flag::Z, (a & val) == 0); setFlag(Flag::N, val & 0x80); setFlag(Flag::V, val & 0x40); }
    void BIT_IMM() { uint8_t val = read(addrIMM()); setFlag(Flag::Z, (a & val) == 0); }
    // 65C02 : BIT zpx et abx (affectent N et V contrairement à BIT imm)
    void BIT_ZPX() { uint8_t val = read(addrZPX()); setFlag(Flag::Z, (a & val) == 0); setFlag(Flag::N, val & 0x80); setFlag(Flag::V, val & 0x40); }
    void BIT_ABX() { uint8_t val = read(addrABX()); setFlag(Flag::Z, (a & val) == 0); setFlag(Flag::N, val & 0x80); setFlag(Flag::V, val & 0x40); }
    // 65C02 : INC A et DEC A
    void INC_A() { a++; setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
    void DEC_A() { a--; setFlag(Flag::Z, a == 0); setFlag(Flag::N, a & 0x80); }
    // 65C02 : JMP (abs,X)
    void JMP_ABSX() { pc = addrABX_IND(); }

    // --- Décodage ---
    using InstrFn = void (CPU65C02::*)();
    std::array<InstrFn, 256> lookup = {};
    
    void buildLookupTable() {
        lookup.fill(static_cast<InstrFn>(&CPU65C02::NOP));

        lookup[0xA9] = static_cast<InstrFn>(&CPU65C02::LDA_Impl<&CPU65C02::addrIMM>); lookup[0xA5] = static_cast<InstrFn>(&CPU65C02::LDA_Impl<&CPU65C02::addrZPG>);
        lookup[0xB5] = static_cast<InstrFn>(&CPU65C02::LDA_Impl<&CPU65C02::addrZPX>); lookup[0xAD] = static_cast<InstrFn>(&CPU65C02::LDA_Impl<&CPU65C02::addrABS>);
        lookup[0xBD] = static_cast<InstrFn>(&CPU65C02::LDA_Impl<&CPU65C02::addrABX>); lookup[0xB9] = static_cast<InstrFn>(&CPU65C02::LDA_Impl<&CPU65C02::addrABY>);
        lookup[0xA1] = static_cast<InstrFn>(&CPU65C02::LDA_Impl<&CPU65C02::addrIZX>); lookup[0xB1] = static_cast<InstrFn>(&CPU65C02::LDA_Impl<&CPU65C02::addrIZY>);
        
        lookup[0xA2] = static_cast<InstrFn>(&CPU65C02::LDX_Impl<&CPU65C02::addrIMM>); lookup[0xA6] = static_cast<InstrFn>(&CPU65C02::LDX_Impl<&CPU65C02::addrZPG>);
        lookup[0xB6] = static_cast<InstrFn>(&CPU65C02::LDX_Impl<&CPU65C02::addrZPY>); lookup[0xAE] = static_cast<InstrFn>(&CPU65C02::LDX_Impl<&CPU65C02::addrABS>);
        lookup[0xBE] = static_cast<InstrFn>(&CPU65C02::LDX_Impl<&CPU65C02::addrABY>);

        lookup[0xA0] = static_cast<InstrFn>(&CPU65C02::LDY_Impl<&CPU65C02::addrIMM>); lookup[0xA4] = static_cast<InstrFn>(&CPU65C02::LDY_Impl<&CPU65C02::addrZPG>);
        lookup[0xB4] = static_cast<InstrFn>(&CPU65C02::LDY_Impl<&CPU65C02::addrZPX>); lookup[0xAC] = static_cast<InstrFn>(&CPU65C02::LDY_Impl<&CPU65C02::addrABS>);
        lookup[0xBC] = static_cast<InstrFn>(&CPU65C02::LDY_Impl<&CPU65C02::addrABX>);

        lookup[0x85] = static_cast<InstrFn>(&CPU65C02::STA_Impl<&CPU65C02::addrZPG>); lookup[0x95] = static_cast<InstrFn>(&CPU65C02::STA_Impl<&CPU65C02::addrZPX>);
        lookup[0x8D] = static_cast<InstrFn>(&CPU65C02::STA_Impl<&CPU65C02::addrABS>); lookup[0x9D] = static_cast<InstrFn>(&CPU65C02::STA_Impl<&CPU65C02::addrABX>);
        lookup[0x99] = static_cast<InstrFn>(&CPU65C02::STA_Impl<&CPU65C02::addrABY>); lookup[0x81] = static_cast<InstrFn>(&CPU65C02::STA_Impl<&CPU65C02::addrIZX>);
        lookup[0x91] = static_cast<InstrFn>(&CPU65C02::STA_Impl<&CPU65C02::addrIZY>);
        lookup[0x86] = static_cast<InstrFn>(&CPU65C02::STX_Impl<&CPU65C02::addrZPG>); lookup[0x96] = static_cast<InstrFn>(&CPU65C02::STX_Impl<&CPU65C02::addrZPY>); lookup[0x8E] = static_cast<InstrFn>(&CPU65C02::STX_Impl<&CPU65C02::addrABS>);
        lookup[0x84] = static_cast<InstrFn>(&CPU65C02::STY_Impl<&CPU65C02::addrZPG>); lookup[0x94] = static_cast<InstrFn>(&CPU65C02::STY_Impl<&CPU65C02::addrZPX>); lookup[0x8C] = static_cast<InstrFn>(&CPU65C02::STY_Impl<&CPU65C02::addrABS>);

        lookup[0x69] = static_cast<InstrFn>(&CPU65C02::ADC_Impl<&CPU65C02::addrIMM>); lookup[0x65] = static_cast<InstrFn>(&CPU65C02::ADC_Impl<&CPU65C02::addrZPG>); lookup[0x75] = static_cast<InstrFn>(&CPU65C02::ADC_Impl<&CPU65C02::addrZPX>);
        lookup[0x6D] = static_cast<InstrFn>(&CPU65C02::ADC_Impl<&CPU65C02::addrABS>); lookup[0x7D] = static_cast<InstrFn>(&CPU65C02::ADC_Impl<&CPU65C02::addrABX>); lookup[0x79] = static_cast<InstrFn>(&CPU65C02::ADC_Impl<&CPU65C02::addrABY>);
        lookup[0x61] = static_cast<InstrFn>(&CPU65C02::ADC_Impl<&CPU65C02::addrIZX>); lookup[0x71] = static_cast<InstrFn>(&CPU65C02::ADC_Impl<&CPU65C02::addrIZY>);
        
        lookup[0xE9] = static_cast<InstrFn>(&CPU65C02::SBC_Impl<&CPU65C02::addrIMM>); lookup[0xE5] = static_cast<InstrFn>(&CPU65C02::SBC_Impl<&CPU65C02::addrZPG>); lookup[0xF5] = static_cast<InstrFn>(&CPU65C02::SBC_Impl<&CPU65C02::addrZPX>);
        lookup[0xED] = static_cast<InstrFn>(&CPU65C02::SBC_Impl<&CPU65C02::addrABS>); lookup[0xFD] = static_cast<InstrFn>(&CPU65C02::SBC_Impl<&CPU65C02::addrABX>); lookup[0xF9] = static_cast<InstrFn>(&CPU65C02::SBC_Impl<&CPU65C02::addrABY>);
        lookup[0xE1] = static_cast<InstrFn>(&CPU65C02::SBC_Impl<&CPU65C02::addrIZX>); lookup[0xF1] = static_cast<InstrFn>(&CPU65C02::SBC_Impl<&CPU65C02::addrIZY>);

        lookup[0x29] = static_cast<InstrFn>(&CPU65C02::AND_Impl<&CPU65C02::addrIMM>); lookup[0x25] = static_cast<InstrFn>(&CPU65C02::AND_Impl<&CPU65C02::addrZPG>); lookup[0x35] = static_cast<InstrFn>(&CPU65C02::AND_Impl<&CPU65C02::addrZPX>);
        lookup[0x2D] = static_cast<InstrFn>(&CPU65C02::AND_Impl<&CPU65C02::addrABS>); lookup[0x3D] = static_cast<InstrFn>(&CPU65C02::AND_Impl<&CPU65C02::addrABX>); lookup[0x39] = static_cast<InstrFn>(&CPU65C02::AND_Impl<&CPU65C02::addrABY>);
        lookup[0x21] = static_cast<InstrFn>(&CPU65C02::AND_Impl<&CPU65C02::addrIZX>); lookup[0x31] = static_cast<InstrFn>(&CPU65C02::AND_Impl<&CPU65C02::addrIZY>);

        lookup[0x09] = static_cast<InstrFn>(&CPU65C02::ORA_Impl<&CPU65C02::addrIMM>); lookup[0x05] = static_cast<InstrFn>(&CPU65C02::ORA_Impl<&CPU65C02::addrZPG>); lookup[0x15] = static_cast<InstrFn>(&CPU65C02::ORA_Impl<&CPU65C02::addrZPX>);
        lookup[0x0D] = static_cast<InstrFn>(&CPU65C02::ORA_Impl<&CPU65C02::addrABS>); lookup[0x1D] = static_cast<InstrFn>(&CPU65C02::ORA_Impl<&CPU65C02::addrABX>); lookup[0x19] = static_cast<InstrFn>(&CPU65C02::ORA_Impl<&CPU65C02::addrABY>);
        lookup[0x01] = static_cast<InstrFn>(&CPU65C02::ORA_Impl<&CPU65C02::addrIZX>); lookup[0x11] = static_cast<InstrFn>(&CPU65C02::ORA_Impl<&CPU65C02::addrIZY>);

        lookup[0x49] = static_cast<InstrFn>(&CPU65C02::EOR_Impl<&CPU65C02::addrIMM>); lookup[0x45] = static_cast<InstrFn>(&CPU65C02::EOR_Impl<&CPU65C02::addrZPG>); lookup[0x55] = static_cast<InstrFn>(&CPU65C02::EOR_Impl<&CPU65C02::addrZPX>);
        lookup[0x4D] = static_cast<InstrFn>(&CPU65C02::EOR_Impl<&CPU65C02::addrABS>); lookup[0x5D] = static_cast<InstrFn>(&CPU65C02::EOR_Impl<&CPU65C02::addrABX>); lookup[0x59] = static_cast<InstrFn>(&CPU65C02::EOR_Impl<&CPU65C02::addrABY>);
        lookup[0x41] = static_cast<InstrFn>(&CPU65C02::EOR_Impl<&CPU65C02::addrIZX>); lookup[0x51] = static_cast<InstrFn>(&CPU65C02::EOR_Impl<&CPU65C02::addrIZY>);

        lookup[0xC9] = static_cast<InstrFn>(&CPU65C02::CMP_Impl<&CPU65C02::addrIMM>); lookup[0xC5] = static_cast<InstrFn>(&CPU65C02::CMP_Impl<&CPU65C02::addrZPG>); lookup[0xD5] = static_cast<InstrFn>(&CPU65C02::CMP_Impl<&CPU65C02::addrZPX>);
        lookup[0xCD] = static_cast<InstrFn>(&CPU65C02::CMP_Impl<&CPU65C02::addrABS>); lookup[0xDD] = static_cast<InstrFn>(&CPU65C02::CMP_Impl<&CPU65C02::addrABX>); lookup[0xD9] = static_cast<InstrFn>(&CPU65C02::CMP_Impl<&CPU65C02::addrABY>);
        lookup[0xC1] = static_cast<InstrFn>(&CPU65C02::CMP_Impl<&CPU65C02::addrIZX>); lookup[0xD1] = static_cast<InstrFn>(&CPU65C02::CMP_Impl<&CPU65C02::addrIZY>);
        lookup[0xE0] = static_cast<InstrFn>(&CPU65C02::CPX_Impl<&CPU65C02::addrIMM>); lookup[0xE4] = static_cast<InstrFn>(&CPU65C02::CPX_Impl<&CPU65C02::addrZPG>); lookup[0xEC] = static_cast<InstrFn>(&CPU65C02::CPX_Impl<&CPU65C02::addrABS>);
        lookup[0xC0] = static_cast<InstrFn>(&CPU65C02::CPY_Impl<&CPU65C02::addrIMM>); lookup[0xC4] = static_cast<InstrFn>(&CPU65C02::CPY_Impl<&CPU65C02::addrZPG>); lookup[0xCC] = static_cast<InstrFn>(&CPU65C02::CPY_Impl<&CPU65C02::addrABS>);

        lookup[0xE6] = static_cast<InstrFn>(&CPU65C02::INC_Impl<&CPU65C02::addrZPG>); lookup[0xF6] = static_cast<InstrFn>(&CPU65C02::INC_Impl<&CPU65C02::addrZPX>); lookup[0xEE] = static_cast<InstrFn>(&CPU65C02::INC_Impl<&CPU65C02::addrABS>); lookup[0xFE] = static_cast<InstrFn>(&CPU65C02::INC_Impl<&CPU65C02::addrABX>);
        lookup[0xC6] = static_cast<InstrFn>(&CPU65C02::DEC_Impl<&CPU65C02::addrZPG>); lookup[0xD6] = static_cast<InstrFn>(&CPU65C02::DEC_Impl<&CPU65C02::addrZPX>); lookup[0xCE] = static_cast<InstrFn>(&CPU65C02::DEC_Impl<&CPU65C02::addrABS>); lookup[0xDE] = static_cast<InstrFn>(&CPU65C02::DEC_Impl<&CPU65C02::addrABX>);
        
        lookup[0x0A] = static_cast<InstrFn>(&CPU65C02::ASL_Impl<&CPU65C02::addrACC>); lookup[0x06] = static_cast<InstrFn>(&CPU65C02::ASL_Impl<&CPU65C02::addrZPG>); lookup[0x16] = static_cast<InstrFn>(&CPU65C02::ASL_Impl<&CPU65C02::addrZPX>); lookup[0x0E] = static_cast<InstrFn>(&CPU65C02::ASL_Impl<&CPU65C02::addrABS>); lookup[0x1E] = static_cast<InstrFn>(&CPU65C02::ASL_Impl<&CPU65C02::addrABX>);
        lookup[0x4A] = static_cast<InstrFn>(&CPU65C02::LSR_Impl<&CPU65C02::addrACC>); lookup[0x46] = static_cast<InstrFn>(&CPU65C02::LSR_Impl<&CPU65C02::addrZPG>); lookup[0x56] = static_cast<InstrFn>(&CPU65C02::LSR_Impl<&CPU65C02::addrZPX>); lookup[0x4E] = static_cast<InstrFn>(&CPU65C02::LSR_Impl<&CPU65C02::addrABS>); lookup[0x5E] = static_cast<InstrFn>(&CPU65C02::LSR_Impl<&CPU65C02::addrABX>);
        lookup[0x2A] = static_cast<InstrFn>(&CPU65C02::ROL_Impl<&CPU65C02::addrACC>); lookup[0x26] = static_cast<InstrFn>(&CPU65C02::ROL_Impl<&CPU65C02::addrZPG>); lookup[0x36] = static_cast<InstrFn>(&CPU65C02::ROL_Impl<&CPU65C02::addrZPX>); lookup[0x2E] = static_cast<InstrFn>(&CPU65C02::ROL_Impl<&CPU65C02::addrABS>); lookup[0x3E] = static_cast<InstrFn>(&CPU65C02::ROL_Impl<&CPU65C02::addrABX>);
        lookup[0x6A] = static_cast<InstrFn>(&CPU65C02::ROR_Impl<&CPU65C02::addrACC>); lookup[0x66] = static_cast<InstrFn>(&CPU65C02::ROR_Impl<&CPU65C02::addrZPG>); lookup[0x76] = static_cast<InstrFn>(&CPU65C02::ROR_Impl<&CPU65C02::addrZPX>); lookup[0x6E] = static_cast<InstrFn>(&CPU65C02::ROR_Impl<&CPU65C02::addrABS>); lookup[0x7E] = static_cast<InstrFn>(&CPU65C02::ROR_Impl<&CPU65C02::addrABX>);

        lookup[0xAA] = static_cast<InstrFn>(&CPU65C02::TAX); lookup[0xA8] = static_cast<InstrFn>(&CPU65C02::TAY); lookup[0x8A] = static_cast<InstrFn>(&CPU65C02::TXA); lookup[0x98] = static_cast<InstrFn>(&CPU65C02::TYA);
        lookup[0xBA] = static_cast<InstrFn>(&CPU65C02::TSX); lookup[0x9A] = static_cast<InstrFn>(&CPU65C02::TXS);
        lookup[0x48] = static_cast<InstrFn>(&CPU65C02::PHA); lookup[0x08] = static_cast<InstrFn>(&CPU65C02::PHP); lookup[0x68] = static_cast<InstrFn>(&CPU65C02::PLA); lookup[0x28] = static_cast<InstrFn>(&CPU65C02::PLP);
        lookup[0xDA] = static_cast<InstrFn>(&CPU65C02::PHX); lookup[0x5A] = static_cast<InstrFn>(&CPU65C02::PHY); lookup[0xFA] = static_cast<InstrFn>(&CPU65C02::PLX); lookup[0x7A] = static_cast<InstrFn>(&CPU65C02::PLY);
        lookup[0xE8] = static_cast<InstrFn>(&CPU65C02::INX); lookup[0xC8] = static_cast<InstrFn>(&CPU65C02::INY); lookup[0xCA] = static_cast<InstrFn>(&CPU65C02::DEX); lookup[0x88] = static_cast<InstrFn>(&CPU65C02::DEY);
        lookup[0x18] = static_cast<InstrFn>(&CPU65C02::CLC); lookup[0x38] = static_cast<InstrFn>(&CPU65C02::SEC); lookup[0xD8] = static_cast<InstrFn>(&CPU65C02::CLD); lookup[0xF8] = static_cast<InstrFn>(&CPU65C02::SED);
        lookup[0x58] = static_cast<InstrFn>(&CPU65C02::CLI); lookup[0x78] = static_cast<InstrFn>(&CPU65C02::SEI); lookup[0xB8] = static_cast<InstrFn>(&CPU65C02::CLV);
        lookup[0x90] = static_cast<InstrFn>(&CPU65C02::BCC); lookup[0xB0] = static_cast<InstrFn>(&CPU65C02::BCS); lookup[0xF0] = static_cast<InstrFn>(&CPU65C02::BEQ); lookup[0xD0] = static_cast<InstrFn>(&CPU65C02::BNE);
        lookup[0x30] = static_cast<InstrFn>(&CPU65C02::BMI); lookup[0x10] = static_cast<InstrFn>(&CPU65C02::BPL); lookup[0x70] = static_cast<InstrFn>(&CPU65C02::BVS); lookup[0x50] = static_cast<InstrFn>(&CPU65C02::BVC);
        lookup[0x80] = static_cast<InstrFn>(&CPU65C02::BRA);
        lookup[0x4C] = static_cast<InstrFn>(&CPU65C02::JMP_ABS); lookup[0x6C] = static_cast<InstrFn>(&CPU65C02::JMP_IND);
        lookup[0x20] = static_cast<InstrFn>(&CPU65C02::JSR); lookup[0x60] = static_cast<InstrFn>(&CPU65C02::RTS); lookup[0x40] = static_cast<InstrFn>(&CPU65C02::RTI); lookup[0x00] = static_cast<InstrFn>(&CPU65C02::BRK);

        lookup[0x24] = static_cast<InstrFn>(&CPU65C02::BIT_ZPG); lookup[0x2C] = static_cast<InstrFn>(&CPU65C02::BIT_ABS); lookup[0x89] = static_cast<InstrFn>(&CPU65C02::BIT_IMM);
        // 65C02 : BIT zpx / abx
        lookup[0x34] = static_cast<InstrFn>(&CPU65C02::BIT_ZPX); lookup[0x3C] = static_cast<InstrFn>(&CPU65C02::BIT_ABX);
        lookup[0x64] = static_cast<InstrFn>(&CPU65C02::STZ); lookup[0x74] = static_cast<InstrFn>(&CPU65C02::STZ); lookup[0x9C] = static_cast<InstrFn>(&CPU65C02::STZ); lookup[0x9E] = static_cast<InstrFn>(&CPU65C02::STZ);
        lookup[0x04] = static_cast<InstrFn>(&CPU65C02::TSB); lookup[0x0C] = static_cast<InstrFn>(&CPU65C02::TSB); lookup[0x14] = static_cast<InstrFn>(&CPU65C02::TRB); lookup[0x1C] = static_cast<InstrFn>(&CPU65C02::TRB);
        // 65C02 : INC A / DEC A
        lookup[0x1A] = static_cast<InstrFn>(&CPU65C02::INC_A); lookup[0x3A] = static_cast<InstrFn>(&CPU65C02::DEC_A);
        // 65C02 : JMP (abs,X)
        lookup[0x7C] = static_cast<InstrFn>(&CPU65C02::JMP_ABSX);
        // 65C02 : Zero Page Indirect (zp) - les 8 opcodes manquants
        lookup[0x12] = static_cast<InstrFn>(&CPU65C02::ORA_Impl<&CPU65C02::addrIZP>);
        lookup[0x32] = static_cast<InstrFn>(&CPU65C02::AND_Impl<&CPU65C02::addrIZP>);
        lookup[0x52] = static_cast<InstrFn>(&CPU65C02::EOR_Impl<&CPU65C02::addrIZP>);
        lookup[0x72] = static_cast<InstrFn>(&CPU65C02::ADC_Impl<&CPU65C02::addrIZP>);
        lookup[0x92] = static_cast<InstrFn>(&CPU65C02::STA_Impl<&CPU65C02::addrIZP>);
        lookup[0xB2] = static_cast<InstrFn>(&CPU65C02::LDA_Impl<&CPU65C02::addrIZP>);
        lookup[0xD2] = static_cast<InstrFn>(&CPU65C02::CMP_Impl<&CPU65C02::addrIZP>);
        lookup[0xF2] = static_cast<InstrFn>(&CPU65C02::SBC_Impl<&CPU65C02::addrIZP>);
    }
};

// Implémentation de reset() et clock() séparées pour garder le code lisible, 
// mais incluses dans le même header pour éviter les problèmes de liaison.
inline void CPU65C02::reset() {
    a = 0x00; x = 0x00; y = 0x00; sp = 0xFD; status = Flag::U | Flag::I; 
    pc = read(0xFFFC) | (read(0xFFFD) << 8); cycles = 8;
}

inline void CPU65C02::clock() {
    if (cycles == 0) {
        if (nmi_line) {
            nmi_line = false; push(pc >> 8); push(pc & 0xFF); push(status & ~Flag::B | Flag::U);
            setFlag(Flag::I, true); pc = read(0xFFFA) | (read(0xFFFB) << 8); cycles = 7;
        } else if (irq_line && !getFlag(Flag::I)) {
            push(pc >> 8); push(pc & 0xFF); push(status & ~Flag::B | Flag::U);
            setFlag(Flag::I, true); pc = read(0xFFFE) | (read(0xFFFF) << 8); cycles = 7;
        } else {
            uint8_t opcode = fetch();
            (this->*lookup[opcode])();
            
            if (cycles == 0) {
                cycles = 2; 
            }
        }
    }
    cycles--;
}