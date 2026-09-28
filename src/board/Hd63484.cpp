#include "Hd63484.h"
#include "CardBackCache.h"
namespace pokeri {
// Indexed by opcode >> 10.  Lengths count the opcode word; 0 = not a command, -1 = WPTN
// (2 + n words; see wptnCountsBytes), -2 = polyline/polygon (2 + 2n words).
// Verified against the HD63484 User's Manual command table (Hitachi, 1984, p. 173) and the
// per-command "Wn" entries.
static const char *const names[64] = {
    0,"ORG","WPR","RPR", 0,0,"WPTN","RPTN", 0,"DRD","DWT","DMOD", 0,0,0,0,
    0,"RD","WT","MOD", 0,0,"CLR","SCLR", "CPY","CPY","CPY","CPY", "SCPY","SCPY","SCPY","SCPY",
    "AMOVE","RMOVE","ALINE","RLINE", "ARCT","RRCT","APLL","RPLL", "APLG","RPLG","CRCL","ELPS",
    "AARC","RARC","AEARC","REARC", "AFRCT","RFRCT","PAINT","DOT", "PTN","PTN","PTN","PTN",
    "AGCPY","AGCPY","AGCPY","AGCPY", "RGCPY","RGCPY","RGCPY","RGCPY"};
// One decoder shared by the model and validated native header acceptance.
const Hd63484::CommandFormat Hd63484::formats[64] = {
    {0,0x3ff},{3,0x3ff},{2,0x3e0},{1,0x3e0},
    {0,0x3ff},{0,0x3ff},{-1,0x3f0},{2,0x3f0},
    {0,0x3ff},{3,0x3ff},{3,0x3ff},{3,0x3fc},
    {0,0x3ff},{0,0x3ff},{0,0x3ff},{0,0x3ff},
    {0,0x3ff},{1,0x3ff},{2,0x3ff},{2,0x3fc},
    {0,0x3ff},{0,0x3ff},{4,0x3ff},{4,0x3fc},
    {5,0x0fc},{5,0x0fc},{5,0x0fc},{5,0x0fc},
    {5,0x0fc},{5,0x0fc},{5,0x0fc},{5,0x0fc},
    {3,0x3ff},{3,0x3ff},{3,0x300},{3,0x300},
    {3,0x300},{3,0x300},{-2,0x300},{-2,0x300},
    {-2,0x300},{-2,0x300},{2,0x200},{4,0x200},
    {5,0x200},{5,0x200},{7,0x200},{7,0x200},
    {3,0x300},{3,0x300},{1,0x200},{1,0x300},
    {2,0x000},{2,0x000},{2,0x000},{2,0x000},
    {5,0x000},{5,0x000},{5,0x000},{5,0x000},
    {5,0x000},{5,0x000},{5,0x000},{5,0x000},
};
static_assert(sizeof(Hd63484::CommandFormat)==4,"native command decoder stride");
const char *Hd63484::mnemonic(uint16_t opcode) { const char *n = names[opcode >> 10]; return n ? n : "?"; }
int Hd63484::length(uint16_t opcode) { return formats[opcode >> 10].words; }

void Hd63484::flushCard(unsigned reason){if(cardCache)cardCache->flush(*this,reason);}

uint8_t Hd63484::read8(unsigned offset) {
    if(!(offset & 2)) return statusNow();
    flushCard(2);
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

void Hd63484::push(uint16_t word) {
    if(!pendingCount) {
#if defined(POKERI_TIME_LEDGER) && !defined(POKERI_LEDGER_FAST_CACHE)
        if(cardCache)cardCache->wordStart(word);
#endif
        const CommandFormat &format=formats[word>>10];
        int n=format.words;
        // Reserved bits must not silently select a nearby command.
        if(!n || (word & format.reserved)) {
            flushCard();
            status |= CER;
            if(!error) error = "HD63484: invalid command word";
            if(commandLog) commandLog(&word, 1, false);
            return;
        }
        pendingLength=n;
    }
    if(pendingCount<64)pendingWords[pendingCount]=word;
    else {
        if(pendingCount==64){
            pendingSpill.reserve(pendingLength);
            for(unsigned i=0;i<64;++i)pendingSpill.push_back(pendingWords[i]);
        }
        pendingSpill.push_back(word);
    }
    ++pendingCount;
    if(pendingCount==2 && pendingLength<0)
        pendingLength=pendingLength==-1?2+(wptnCountsBytes?word/2:word):2+2*word;
    if(int(pendingCount)!=pendingLength)return;
    const unsigned group=pendingWords[0]>>10;
    if(cardCache && cardCache->command(*this,pendingData(),pendingCount)){finishCommand(group,true);return;}
    // These operations need no general drawing validation or pixel setup.
    if(group==2){
        unsigned pr=pendingWords[0]&31;parameter[pr]=pendingWords[1];
        if(pr==12 || pr==13)rwp=(uint32_t(parameter[12]&255)<<12)|(parameter[13]>>4);
        finishCommand(group,true);
    }else if(group==32 || group==33){
        drawingStopped=false;drawingWork=0;
        int x=int16_t(pendingWords[1]),y=int16_t(pendingWords[2]);
        if(group==33){x+=int16_t(parameter[18]);y+=int16_t(parameter[19]);}
        position(x,y);finishCommand(group,true);
    }else execute();
}

void Hd63484::result(uint16_t word) { readFifo.push_back(word); }

void Hd63484::execute() {
    const uint16_t *words=pendingData();
    const uint16_t op=words[0], *p=words+1;
    const unsigned group = op >> 10;
    bool done = true;
    auto syncRwp = [this] {
        parameter[0x0c] = uint16_t((parameter[0x0c] & 0xff00) | ((rwp >> 12) & 0xff));
        parameter[0x0d] = uint16_t(((rwp & 0xfff) << 4) | (parameter[0x0d] & 0xf));
    };
    switch(group) {
    case 1: origin = uint32_t(p[0]) << 16 | p[1]; position(0, 0); break;                      // ORG
    case 3: result(parameter[op & 0x1f]); status &= ~ARD; break;                               // RPR
    case 17: result(readWord(rwp)); rwp = (rwp + 1) & 0xfffff; syncRwp(); break;  // RD
    case 18: writeWord(rwp,p[0]); rwp = (rwp + 1) & 0xfffff; syncRwp(); break;   // WT
    case 19: {                                                                  // MOD
        // MM: 00 replace, 01 OR, 10 AND, 11 EOR; only bits set in MASK (PR04) change (UM 6.5.1).
        uint16_t w = readWord(rwp), m = parameter[4], v;
        switch(op & 3) { case 0: v = p[0]; break; case 1: v = w | p[0]; break; case 2: v = w & p[0]; break; default: v = w ^ p[0]; }
        writeWord(rwp,uint16_t((w & ~m) | (v & m)));
        rwp = (rwp + 1) & 0xfffff; syncRwp();
        break;
    }
    default:
        done = draw(op, p);
        if(group == 22) syncRwp();
        if(!done) ++unexecuted;
        break;
    }
    finishCommand(group,done);
}
void Hd63484::finishCommand(unsigned group,bool done){
    ++commands[group];
    if(commandLog)commandLog(pendingData(),pendingCount,done);
    clearPending();status|=CED;
}
}
