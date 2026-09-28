#ifndef POKERI_HD63484_H
#define POKERI_HD63484_H
#include <array>
#include <cstdint>
#include <deque>
#include <vector>
#include <utility>
#include "Device.h"
#include "State.h"
#include "Surface.h"
namespace pokeri {
class CardBackCache;
// Hitachi HD63484 ACRTC on an 8-bit host bus (docs/rom-set.md, "HD63484").
// Offset bit 1 is RS: 0 = address register write / status read, 1 = data.
// Commands execute the moment their last word arrives, so the write FIFO is always
// ready and empty afterwards. Drawing is synchronous, not cycle accurate.
// Only implemented commands/modes succeed; unsupported operations set CER and error.
// CCR bits 10-8 (GBM) set the bits per pixel; the ROM selects 4 bpp (CCR high byte $02).
struct Hd63484 : Device {
    // Status bits; CCR low byte enables the matching interrupt bit for bit (CRE ARE CEE LPE RFE
    // RRE WRE WEE).  Source: Hitachi ACRTC Application Note (1986), vol. 2 fig. 7-6(f), vol. 3 §1.3.
    enum : uint8_t { WFE=0x01, WFR=0x02, RFR=0x04, RFF=0x08, LPD=0x10, CED=0x20, ARD=0x40, CER=0x80 };
    uint8_t ar = 0;
    std::array<uint8_t, 256> control{};        // byte-addressed registers, AR >= 2
    std::array<uint16_t, 32> parameter{};      // WPR/RPR drawing parameter registers
    std::array<uint16_t, 16> pattern{};          // 16 by 16 pattern RAM
    std::vector<uint16_t> frame;               // word-addressed frame memory
    // Installed video memory as an address mask (words).  The ROM probes it ($5BC8) and takes a
    // different path depending on aliasing.  Default 256K words = 512 KB: the variant our machine
    // uses (user, 2026-09-24).  2 MB (0xfffff) additionally programs a RAMDAC at $E0000.
    uint32_t frameMask = 0x3ffff;
    // WPTN's count n in bytes rather than words.  The User's Manual (p. 181) requires bytes on
    // an 8-bit bus, but this ROM sends n in WORDS: WPTN $1800 n=16 and $1802 n=14 each fill the
    // 16-word pattern RAM exactly, and the next command follows n words later
    // (docs/rom-set.md).  Off by default; kept as a switch for the unresolved bus question.
    bool wptnCountsBytes = false;
    uint32_t rwp = 0;                          // read/write pointer, a 20-bit word address
    uint32_t origin = 0;                       // ORG drawing origin, as written
    uint8_t status = WFR | WFE | CED;
    bool presentationBusy=false; // live port pacing; serialized by its policy, not chip snapshots
#ifdef POKERI_FREESTANDING
    using CommandCount=uint32_t; // Native diagnostics only; modulo 2^32, no chip effect.
#else
    using CommandCount=uint64_t; // Preserve host reports and snapshot wire format.
#endif
    std::array<CommandCount,64> commands{};   // executed + parsed commands by opcode >> 10
    uint64_t unexecuted = 0, readUnderflows = 0;
    // Runtime-only memoization statistics; neither counters nor cache are chip state.
    uint32_t curveCacheHits=0,curveCacheMisses=0;
    const char *error = nullptr;                // first protocol violation, if any
    void (*commandLog)(const uint16_t *words, unsigned count, bool executed) = nullptr;

    friend class CardBackCache;
    CardBackCache *cardCache=nullptr;
    bool cachedPixels=false;
    void flushCard(unsigned reason=0);
    uint32_t drawingWorkCount()const{return drawingWork;}
    bool drawingFailed()const{return drawingStopped;}
    void observePixels()const {if(cachedPixels)const_cast<Hd63484*>(this)->flushCard(4);}
    Surface *surface=nullptr; // runtime attachment; host reference uses frame
    uint16_t readWord(uint32_t address)const{observePixels();address&=frameMask;return surface?surface->readWord(address):frame[address];}
    void writeWord(uint32_t address,uint16_t value){observePixels();address&=frameMask;if(surface)surface->writeWord(address,value);else frame[address]=value;}
    void state(State &s);
#ifdef POKERI_FREESTANDING
    Hd63484(bool=true) {} // Amiga attaches CHIP bitplanes before executing the ROM.
#else
    Hd63484(bool allocateFrame=true) : frame(allocateFrame?1u << 20:0) {}
#endif
    // Stable authoritative fields for the native address-port MOVE handler.
    // Selecting a register changes no status/IRQ/command state. The caller
    // must invalidate every borrowed FIFO span before returning to the guest.
    struct AddressSelector {uint8_t *address;bool *writePhase,*readPhase;};
    AddressSelector addressSelector(){return {&ar,&writeLow,&readLow};}
    uint8_t read8(unsigned offset) override;
    // Kept visible for validated fixed-endpoint callers; this is the same
    // authoritative byte protocol used by the generic Board bus.
    void write8(unsigned offset, uint8_t value) override {
        if(!(offset & 2)) { ar = value; writeLow = readLow = false; return; }
        if(ar < 2) {                                      // write FIFO, high byte first
            if(presentationBusy){error="FIFO write during presentation hold";return;}
            if(!writeLow) { writeHigh = value; writeLow = true; return; }
            writeLow = false;
            push(uint16_t(writeHigh << 8 | value));
            return;
        }
        // CCR low only changes interrupt enables, never pixels or drawing
        // context. The ROM toggles it between batches of the same card.
        if(ar!=3)flushCard(3);
        control[ar] = value;
        if(ar == 2 && (value & 0x80)) {                   // CCR ABT: abort the command in progress
            clearPending(); readFifo.clear(); status = CED;
        }
        if(ar >= 0x80) ++ar;
    }
    // Same two high-first data bytes, with one FIFO/address decode. A partial
    // byte may complete an earlier word and leave a new high byte pending.
    // Control-register words retain their byte-by-byte auto-increment/ABT path.
    bool writeFifoWord(uint16_t value){
        if(ar>=2)return false;
        if(presentationBusy){error="FIFO write during presentation hold";return true;}
        if(writeLow){
            writeLow=false;
            push(uint16_t(uint16_t(writeHigh)<<8 | (value>>8)));
            writeHigh=uint8_t(value);writeLow=true;
        }else {writeHigh=uint8_t(value>>8);push(value);}
        return true;
    }
    // Borrow only intermediate, known-length inline parameters. The caller
    // must end the borrow before any other device operation, byte access,
    // reset or observation barrier. Opcode, variable count, final word and
    // spilled commands always go through writeFifoWord/push. Updating these
    // fields has no command/status/IRQ side effect; snapshots remain exact.
    unsigned inlineParameters(uint16_t *&words,unsigned *&count,uint8_t *&high){
        if(ar>=2 || writeLow || presentationBusy || error || !pendingCount ||
           pendingLength<=0 || pendingLength>64 || unsigned(pendingLength)<=pendingCount+1)return 0;
        words=pendingWords+pendingCount;count=&pendingCount;high=&writeHigh;
        return unsigned(pendingLength)-pendingCount-1;
    }
    // Empty-command grant: only headers whose acceptance cannot change an
    // enabled CED IRQ. The caller validates formats, excludes one-word commands,
    // updates all four fields, and ends the borrow at every observation barrier.
    bool inlineHeader(uint16_t *&words,unsigned *&count,uint8_t *&high,int *&length){
        if(ar>=2 || writeLow || presentationBusy || error || pendingCount || (control[3]&CED))return false;
        words=pendingWords;count=&pendingCount;high=&writeHigh;length=&pendingLength;
        return true;
    }
    struct CommandFormat {int16_t words;uint16_t reserved;};
    static const CommandFormat formats[64];
    void tick(uint32_t) override {}
    bool irq() const override { return (statusNow() & control[3]) != 0; }
    uint8_t statusNow() const {
        uint8_t s = status & (CED | CER | ARD | LPD);
        if(!presentationBusy)s |= WFE | WFR; // presentation holds backpressure the guest ring
        if(pendingCount) s &= ~CED;
        if(!readFifo.empty()) s |= RFR;
        if(readFifo.size() >= 8) s |= RFF;
        return s;
    }
    static const char *mnemonic(uint16_t opcode);
    static int length(uint16_t opcode);        // words including the opcode; <0 = variable

private:
    bool writeLow = false, readLow = false;
    uint8_t writeHigh = 0;
    uint16_t readLatch = 0;
    // All fixed commands and normal small polygons stay inline. Large legal
    // variable commands spill, preserving their existing parameter/fault rules.
    uint16_t pendingWords[64];
    std::vector<uint16_t> pendingSpill;
    unsigned pendingCount=0;
    int pendingLength=0;
    uint16_t *pendingData(){return pendingCount<=64?pendingWords:pendingSpill.data();}
    void clearPending(){pendingCount=0;pendingLength=0;pendingSpill.clear();}
    void finishCommand(unsigned group,bool done);
    std::deque<uint16_t> readFifo;
    void push(uint16_t word);
    void execute();
    void result(uint16_t word);
    bool draw(uint16_t op, const uint16_t *p);
    void fail(const char *reason);
    unsigned memoryWidth(unsigned dn) const;
    unsigned bpp() const;
    uint32_t pixelAddress(int x, int y, unsigned &shift) const;
    uint16_t pixel(int x, int y) const;
    void position(int x, int y);
    bool solidPattern(uint16_t op,uint16_t &color)const;
    bool rectangle(uint16_t op,int left,int top,unsigned width,unsigned height,uint16_t color);
    bool plot(uint16_t op, int x, int y, uint16_t color);
    bool plotAt(uint16_t op,uint32_t address,unsigned shift,unsigned depth,uint16_t color);
    bool patterned(uint16_t op, int x, int y, int px, int py);
    uint16_t patternPoint(int px, int py) const;
    void line(uint16_t op, int x, int y, int ex, int ey, int &phase);
    void curve(uint16_t op, int cx, int cy, unsigned coefficientX, unsigned coefficientY,
               uint64_t radius, int startX, int startY, bool closed, int ex, int ey);
    void paint(uint16_t op);
    mutable CpuPlanes cpuPlanes;
    mutable bool cpuAccessTried=false;
    void invalidateCpu(){cpuAccessTried=false;cpuPlanes.data=nullptr;}
    CpuPlanes *cpuAccess()const{
        if(!cpuAccessTried){cpuAccessTried=true;if(surface)surface->cpuAccess4(cpuPlanes);}
        return cpuPlanes.data?&cpuPlanes:nullptr;
    }
    uint16_t planePixel(uint32_t a,unsigned shift)const{
        if(auto *cpu=cpuAccess())return cpu->pixel4(a,shift);
        return surface->pixel4(a,shift);
    }
    void planePlot(uint32_t a,unsigned shift,unsigned color,unsigned op){
        if(auto *cpu=cpuAccess())cpu->plot4(a,shift,color,op);
        else surface->plot4(a,shift,color,op);
    }
    bool planeRead(uint32_t a,uint16_t *out)const{
        if(auto *cpu=cpuAccess()){cpu->readPlanes4(a,out);return true;}
        return surface->readPlanes4(a,out);
    }
    struct CurveKey {
        uint64_t radius;
        unsigned coefficientX,coefficientY;
        int startX,startY,finishX,finishY;
        unsigned flags;
        bool operator==(const CurveKey &other)const {
            return radius==other.radius && coefficientX==other.coefficientX && coefficientY==other.coefficientY &&
                startX==other.startX && startY==other.startY && finishX==other.finishX && finishY==other.finishY && flags==other.flags;
        }
    };
    struct CurveEntry {
        CurveKey key{};
        std::vector<std::pair<int,int>> points;
        std::array<std::vector<CurveWord>,16> stamps;
        int minX=0,maxX=0,minY=0,maxY=0;
        bool valid=false;
    };
    bool stampCurve(uint16_t op,int cx,int cy,CurveEntry &entry);
    // Eight lazily populated outlines, at most 512 points each (32 KB total).
    // Up to 16 lazy alignments, at most 512 eight-byte mask records each.
    // Coordinates stay relative; patterns, colours, addressing and ROPs are
    // evaluated on every draw. Large/rare outlines retain the uncached path.
    std::array<CurveEntry,8> curveCache;
    unsigned nextCurveEntry=0;
    // Per-command selector for a repeating one-row, unzoomed-X pattern.
    bool repeatingPattern=false;
    uint16_t repeatingBits=0;
    bool drawingStopped = false;
    uint32_t drawingWork = 0;
    bool work();
};
}
#endif
