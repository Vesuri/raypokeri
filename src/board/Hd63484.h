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
    std::array<uint64_t, 64> commands{};       // executed + parsed commands by opcode >> 10
    uint64_t unexecuted = 0, readUnderflows = 0;
    // Runtime-only memoization statistics; neither counters nor cache are chip state.
    uint32_t curveCacheHits=0,curveCacheMisses=0;
    const char *error = nullptr;                // first protocol violation, if any
    void (*commandLog)(const uint16_t *words, unsigned count, bool executed) = nullptr;

    Surface *surface=nullptr; // runtime attachment; host reference uses frame
    uint16_t readWord(uint32_t address)const{address&=frameMask;return surface?surface->readWord(address):frame[address];}
    void writeWord(uint32_t address,uint16_t value){address&=frameMask;if(surface)surface->writeWord(address,value);else frame[address]=value;}
    void state(State &s);
#ifdef POKERI_FREESTANDING
    Hd63484() {} // Amiga attaches CHIP bitplanes before executing the ROM.
#else
    Hd63484() : frame(1u << 20) {}
#endif
    uint8_t read8(unsigned offset) override;
    // Kept visible for validated fixed-endpoint callers; this is the same
    // authoritative byte protocol used by the generic Board bus.
    void write8(unsigned offset, uint8_t value) override {
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
    void tick(uint32_t) override {}
    bool irq() const override { return (statusNow() & control[3]) != 0; }
    uint8_t statusNow() const {
        uint8_t s = status & (CED | CER | ARD | LPD);
        s |= WFE | WFR; // commands never queue
        if(!pending.empty()) s &= ~CED;
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
    std::vector<uint16_t> pending;
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
        bool valid=false;
    };
    // Eight lazily populated outlines, at most 512 points each (32 KB total).
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
