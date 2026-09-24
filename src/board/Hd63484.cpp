#include "Hd63484.h"
namespace pokeri {
// Indexed by opcode >> 10.  Lengths count the opcode word; 0 = not a command, -1 = WPTN
// (2 + n words), -2 = polyline/polygon (2 + 2n words).  From the HD63484 command set.
static const char *const names[64] = {
    0,"ORG","WPR","RPR", 0,0,"WPTN","RPTN", 0,"DRD","DWT","DMOD", 0,0,0,0,
    0,"RD","WT","MOD", 0,0,"CLR","SCLR", "CPY","CPY","CPY","CPY", "SCPY","SCPY","SCPY","SCPY",
    "AMOVE","RMOVE","ALINE","RLINE", "ARCT","RRCT","APLL","RPLL", "APLG","RPLG","CRCL","ELPS",
    "AARC","RARC","AEARC","REARC", "AFRCT","RFRCT","PAINT","DOT", "PTN","PTN","PTN","PTN",
    "AGCPY","AGCPY","AGCPY","AGCPY", "RGCPY","RGCPY","RGCPY","RGCPY"};
static const signed char lengths[64] = {
    0,3,2,1, 0,0,-1,2, 0,3,3,3, 0,0,0,0,
    0,1,2,2, 0,0,4,4, 5,5,5,5, 5,5,5,5,
    3,3,3,3, 3,3,-2,-2, -2,-2,2,4,
    5,5,7,7, 3,3,1,1, 2,2,2,2,
    5,5,5,5, 5,5,5,5};

const char *Hd63484::mnemonic(uint16_t opcode) { const char *n = names[opcode >> 10]; return n ? n : "?"; }
int Hd63484::length(uint16_t opcode) { return lengths[opcode >> 10]; }

uint8_t Hd63484::statusNow() const {
    uint8_t s = status & (CED | CER | ARD | LPD);
    s |= WFE | WFR;                                   // commands never queue
    if(!pending.empty()) s &= ~CED;
    if(!readFifo.empty()) s |= RFR;
    if(readFifo.size() >= 8) s |= RFF;
    return s;
}

uint8_t Hd63484::read8(unsigned offset) {
    if(!(offset & 2)) return statusNow();
    if(ar < 2) {                                      // read FIFO, high byte first
        if(readLow) { readLow = false; return uint8_t(readLatch); }
        if(readFifo.empty()) { ++readUnderflows; readLatch = 0; }
        else { readLatch = readFifo.front(); readFifo.pop_front(); }
        readLow = true;
        return uint8_t(readLatch >> 8);
    }
    uint8_t v = control[ar];
    if(ar >= 0x80) ++ar;                              // display registers auto-increment per byte
    return v;
}

void Hd63484::write8(unsigned offset, uint8_t value) {
    if(!(offset & 2)) { ar = value; writeLow = readLow = false; return; }
    if(ar < 2) {                                      // write FIFO, high byte first
        if(!writeLow) { writeHigh = value; writeLow = true; return; }
        writeLow = false;
        push(uint16_t(writeHigh << 8 | value));
        return;
    }
    control[ar] = value;
    if(ar == 2 && (value & 0x80)) {                   // CCR ABT: abort the command in progress
        pending.clear(); readFifo.clear(); status = CED;
    }
    if(ar >= 0x80) ++ar;
}

void Hd63484::push(uint16_t word) {
    if(pending.empty()) {
        int n = length(word);
        if(!n) {
            status |= CER;
            if(!error) error = "HD63484: invalid command word";
            if(commandLog) commandLog(&word, 1, false);
            return;
        }
    }
    pending.push_back(word);
    int n = length(pending[0]);
    if(n == -1) n = pending.size() >= 2 ? 2 + pending[1] : 0;
    else if(n == -2) n = pending.size() >= 2 ? 2 + 2 * pending[1] : 0;
    if(n && int(pending.size()) >= n) execute();
}

void Hd63484::result(uint16_t word) { readFifo.push_back(word); }

void Hd63484::execute() {
    const uint16_t op = pending[0], *p = pending.data() + 1;
    const unsigned group = op >> 10;
    bool done = true;
    auto syncRwp = [this] {
        parameter[0x0c] = uint16_t((parameter[0x0c] & 0xff00) | ((rwp >> 12) & 0xff));
        parameter[0x0d] = uint16_t(((rwp & 0xfff) << 4) | (parameter[0x0d] & 0xf));
    };
    switch(group) {
    case 1: origin = uint32_t(p[0]) << 16 | p[1]; break;                      // ORG
    case 2:                                                                     // WPR
        parameter[op & 0x1f] = p[0];
        if((op & 0x1f) == 0x0c || (op & 0x1f) == 0x0d)
            rwp = (uint32_t(parameter[0x0c] & 0xff) << 12) | (parameter[0x0d] >> 4);
        break;
    case 3: result(parameter[op & 0x1f]); break;                               // RPR
    case 17: result(frame[rwp & frameMask]); rwp = (rwp + 1) & 0xfffff; syncRwp(); break;  // RD
    case 18: frame[rwp & frameMask] = p[0]; rwp = (rwp + 1) & 0xfffff; syncRwp(); break;   // WT
    case 19: {                                                                  // MOD
        uint16_t &w = frame[rwp & frameMask];
        switch(op & 3) { case 0: w = p[0]; break; case 1: w |= p[0]; break; case 2: w &= p[0]; break; default: w ^= p[0]; }
        rwp = (rwp + 1) & 0xfffff; syncRwp();
        break;
    }
    default: done = false; ++unexecuted; break;                                // drawing: Phase 2
    }
    ++commands[group];
    if(commandLog) commandLog(pending.data(), pending.size(), done);
    pending.clear();
    status = uint8_t((status & ~CER) | CED);
}
}
