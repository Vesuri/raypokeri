#include "Hd63484.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace pokeri {
// Semantics: Hitachi HD63484 User's Manual (1984), chapters 5/6 and command
// sheets. MAME's BSD-3-Clause device was consulted as a cross-check; see
// docs/rom-set.md for model limits. No original program/graphics data here.
void Hd63484::fail(const char *reason) {
    if(!error) error = reason;
    status |= CER;
    drawingStopped = true;
}
unsigned Hd63484::memoryWidth(unsigned dn) const {
    unsigned a = 0xc2 + 8 * dn;
    return ((unsigned(control[a]) << 8) | control[a+1]) & 0xfff;
}
unsigned Hd63484::bpp() const {
    unsigned mode = control[2] & 7;
    return mode <= 4 ? 1u << mode : 1;
}
uint32_t Hd63484::pixelAddress(int x, int y, unsigned &shift) const {
    int bits = bpp(), ppw = 16 / bits;
    int dot = int16_t(x) + int(origin & 15) / bits;
    // C++ division truncates; physical addressing needs floor for negative X.
    int word = dot / ppw, rem = dot % ppw;
    if(rem < 0) { --word; rem += ppw; }
    shift = unsigned(rem * bits);
    return (uint32_t(int64_t((origin >> 4) & 0xfffff) + word -
                     int64_t(int16_t(y)) * memoryWidth(origin >> 30))) & 0xfffff;
}
uint16_t Hd63484::pixel(int x, int y) const {
    unsigned shift;
    uint32_t address = pixelAddress(x, y, shift);
    return (frame[address & frameMask] >> shift) & ((1u << bpp()) - 1);
}
void Hd63484::position(int x, int y) {
    parameter[0x12] = uint16_t(x);
    parameter[0x13] = uint16_t(y);
    unsigned shift;
    uint32_t a = pixelAddress(x, y, shift);
    parameter[0x10] = uint16_t((origin >> 16 & 0xc000) | (a >> 12));
    parameter[0x11] = uint16_t((a << 4) | shift);
}
bool Hd63484::work() {
    // Diagnostic bound, not a simulated chip limit. Never report a runaway as success.
    if(++drawingWork > 4u * 1024 * 1024) fail("HD63484: drawing work limit (open PAINT boundary or excessive dimensions)");
    return !drawingStopped;
}
bool Hd63484::plot(uint16_t op, int x, int y, uint16_t color) {
    if(!work()) return false;
    unsigned shift;
    uint16_t &dest = frame[pixelAddress(x, y, shift) & frameMask];
    uint16_t mask = uint16_t(((1u << bpp()) - 1) << shift);
    uint16_t src = uint16_t(color << shift) & mask;
    switch(op & 7) {
    case 0: dest = (dest & ~mask) | src; break;
    case 1: dest |= src; break;
    case 2: dest &= uint16_t(~mask | src); break;
    case 3: dest ^= src; break;
    default: fail("HD63484: unsupported conditional drawing operation"); return false;
    }
    return true;
}
static int positiveMod(int n, int d) { int r = n % d; return r < 0 ? r + d : r; }
uint16_t Hd63484::patternPoint(int px, int py) const {
    uint16_t result = 0;
    for(unsigned axis = 0; axis < 2; ++axis) {
        unsigned s = axis * 8;
        int start = (parameter[6] >> (s+4)) & 15;
        int end = (parameter[7] >> (s+4)) & 15;
        int zoom = ((parameter[7] >> s) & 15) + 1;
        int point = (parameter[5] >> (s+4)) & 15;
        int count = (parameter[5] >> s) & 15;
        int n = positiveMod((point-start)*zoom + count + (axis ? py : px), (end-start+1)*zoom);
        result |= uint16_t(((start + n/zoom) << (s+4)) | ((n%zoom) << s));
    }
    return result;
}
bool Hd63484::patterned(uint16_t op, int x, int y, int px, int py) {
    uint16_t pp = patternPoint(px, py);
    bool bit = (pattern[pp >> 12] >> ((pp >> 4) & 15)) & 1;
    unsigned col = (op >> 3) & 3;
    if((col == 1 && !bit) || (col == 2 && bit)) return work();
    unsigned shift;
    pixelAddress(x, y, shift);
    return plot(op, x, y, (parameter[bit ? 1 : 0] >> shift) & ((1u << bpp())-1));
}
void Hd63484::line(uint16_t op, int x, int y, int ex, int ey, int &phase) {
    int dx = std::abs(ex-x), dy = std::abs(ey-y);
    int sx = ex < x ? -1 : 1, sy = ey < y ? -1 : 1;
    int major = std::max(dx, dy), minor = std::min(dx, dy);
    int err = 2*minor-major;
    for(int i = 0; i < major && !drawingStopped; ++i) {
        patterned(op, x, y, phase++, 0); // end point excluded, including zero-length lines
        if(err >= 0) { if(dx > dy) y += sy; else x += sx; err -= 2*major; }
        if(dx > dy) x += sx; else y += sy;
        err += 2*minor;
    }
}
void Hd63484::curve(uint16_t op, int cx, int cy, double rx, double ry,
                    double start, double sweep, bool closed, int ex, int ey) {
    // Midpoint ellipse raster, traversed from the requested start in C direction.
    // The manual specifies the conic and endpoints, not silicon's pixel ties.
    // Each pixel is emitted once (important for XOR and line pattern progression).
    if(!std::isfinite(rx) || !std::isfinite(ry) || rx > 32767 || ry > 32767) {
        fail("HD63484: excessive curve dimensions"); return;
    }
    std::set<std::pair<int,int>> outline;
    auto symmetric = [&](int x, int y) {
        for(int sx : {-1,1}) for(int sy : {-1,1})
            outline.insert(std::make_pair(sx*x,sy*y));
    };
    double a=rx*rx,b=ry*ry;
    int qx=0,qy=int(std::round(ry));
    double dx=0,dy=2*a*qy;
    double decision=b-a*qy+a/4;
    while(dx<dy) {
        symmetric(qx,qy);
        ++qx; dx+=2*b;
        if(decision<0) decision+=dx+b;
        else { --qy; dy-=2*a; decision+=dx-dy+b; }
    }
    decision=b*(qx+0.5)*(qx+0.5)+a*(qy-1.0)*(qy-1.0)-a*b;
    while(qy>=0) {
        symmetric(qx,qy);
        --qy; dy-=2*a;
        if(decision>0) decision+=a-dy;
        else { ++qx; dx+=2*b; decision+=dx-dy+a; }
    }
    const double tau=6.2831853071795864769;
    std::vector<std::pair<double,std::pair<int,int>>> ordered;
    for(const auto &q : outline) {
        double angle=std::atan2(ry ? q.second/ry : 0, rx ? q.first/rx : 0);
        double travel=std::fmod((sweep<0 ? start-angle : angle-start)+2*tau,tau);
        if(closed || travel < std::abs(sweep)) ordered.push_back(std::make_pair(travel,q));
    }
    std::sort(ordered.begin(),ordered.end());
    int phase=0;
    for(const auto &q : ordered) {
        int x=cx+q.second.first,y=cy+q.second.second;
        if(!closed && x==ex && y==ey) continue;
        if(!patterned(op,x,y,phase++,0)) break;
    }
}
void Hd63484::paint(uint16_t op) {
    // Scanline fill; four pending seeds is the documented internal stack limit.
    // Overflow is a loud stop until suspend/resume through the read FIFO is modeled.
    int sx = int16_t(parameter[0x12]), sy = int16_t(parameter[0x13]);
    std::vector<std::pair<int,int>> seeds(1, std::make_pair(sx,sy));
    std::set<std::pair<int,int>> visited;
    auto eligible = [&](int x, int y) {
        if(!work()) return false;
        if(x < -32768 || x > 32767 || y < -32768 || y > 32767) {
            fail("HD63484: PAINT reached coordinate wrap"); return false;
        }
        if(visited.count(std::make_pair(x,y))) return false;
        unsigned shift; pixelAddress(x,y,shift);
        unsigned mask = (1u << bpp())-1, d = pixel(x,y);
        unsigned edge = (parameter[3] >> shift) & mask;
        return d != ((parameter[0] >> shift) & mask) && d != ((parameter[1] >> shift) & mask)
            && ((op & 0x100) ? d == edge : d != edge);
    };
    while(!seeds.empty() && !drawingStopped) {
        int x = seeds.back().first, y = seeds.back().second;
        seeds.pop_back();
        if(!eligible(x,y)) continue;
        int left = x, right = x;
        while(eligible(left-1,y)) --left;
        while(eligible(right+1,y)) ++right;
        for(int px = left; px <= right && !drawingStopped; ++px) {
            visited.insert(std::make_pair(px,y));
            patterned(op, px,y,px-sx,y-sy);
            position(px,y);
        }
        for(int dir : {-1,1}) {
            bool inRun = false;
            for(int px = left; px <= right && !drawingStopped; ++px) {
                bool on = eligible(px,y+dir);
                if(on && !inRun) {
                    if(seeds.size() == 4) { fail("HD63484: PAINT seed stack overflow needs read-FIFO continuation"); break; }
                    seeds.push_back(std::make_pair(px,y+dir));
                }
                inRun = on;
            }
        }
    }
}
bool Hd63484::draw(uint16_t op, const uint16_t *p) {
    drawingStopped = false; drawingWork = 0;
    unsigned group = op >> 10;
    int x = int16_t(parameter[0x12]), y = int16_t(parameter[0x13]);
    if(group == 6) {
        unsigned n = wptnCountsBytes ? p[0]/2 : p[0];
        if((op & 0x3f0) || n > 16 || (op & 15)+n > 16 || (wptnCountsBytes && (p[0]&1)))
            fail("HD63484: unsupported WPTN range/count");
        else for(unsigned i=0; i<n; ++i) pattern[(op&15)+i] = p[1+i];
        return !drawingStopped;
    }
    if(group == 22) {
        if(op != 0x5800) { fail("HD63484: invalid CLR mode"); return false; }
        int ax = int16_t(p[1]), ay = int16_t(p[2]);
        int sx = ax < 0 ? -1 : 1, sy = ay < 0 ? -1 : 1;
        unsigned mw = memoryWidth(parameter[0xc] >> 14);
        for(int j=0; j<=std::abs(ay) && !drawingStopped; ++j)
            for(int i=0; i<=std::abs(ax) && work(); ++i)
                frame[uint32_t(int64_t(rwp)+i*sx-int64_t(j*sy)*mw) & frameMask] = p[0];
        if(!drawingStopped) rwp = uint32_t(int64_t(rwp)-int64_t(ay+sy)*mw) & 0xfffff;
        return !drawingStopped;
    }
    if(group == 32 || group == 33) {
        if(op & 0x3ff) fail("HD63484: invalid MOVE mode");
        else position(int16_t(p[0]) + (group==33 ? x : 0), int16_t(p[1]) + (group==33 ? y : 0));
        return !drawingStopped;
    }
    bool supported = group==35 || group==38 || group==39 || group==42 || group==43 ||
                     group==45 || group==47 || group==49 || group==50 || group==51 ||
                     (group>=52 && group<=59);
    if(!supported) { fail("HD63484: unimplemented command"); return false; }
    if((control[2]&7)>4) { fail("HD63484: invalid graphic bit mode"); return false; }
    // The measured boot uses AREA=0, COL=0/1, OPM=replace/OR. Other known
    // logical operations below share the same pixel primitive; unknown modes stop.
    if(op & 0xe0) { fail("HD63484: unsupported drawing area mode"); return false; }
    if((op&7)>3 || ((op>>3)&3)==3) { fail("HD63484: unsupported drawing color/operation mode"); return false; }
    if(group < 56) for(unsigned s : {0u,8u}) {
        int a=(parameter[6]>>(s+4))&15, b=(parameter[7]>>(s+4))&15;
        int pos=(parameter[5]>>(s+4))&15;
        if(a>b || pos<a || pos>b || ((parameter[5]>>s)&15)>((parameter[7]>>s)&15)) {
            fail("HD63484: invalid pattern bounds/pointer/zoom"); return false;
        }
    }
    int phase=0;
    switch(group) {
    case 35: {
        int ex=int16_t(x+int16_t(p[0])), ey=int16_t(y+int16_t(p[1]));
        line(op,x,y,ex,ey,phase); if(!drawingStopped) position(ex,ey); break;
    }
    case 38: case 39:
        for(unsigned i=0;i<p[0] && !drawingStopped;++i) {
            int ex=int16_t(p[1+2*i]+(group==39?x:0));
            int ey=int16_t(p[2+2*i]+(group==39?y:0));
            line(op,x,y,ex,ey,phase); x=ex; y=ey;
        }
        if(!drawingStopped) position(x,y);
        break;
    case 42: case 43: case 45: case 47: {
        const double tau=6.2831853071795864769;
        double rx,ry,start=0,sweep=(op&0x100)?-tau:tau;
        int cx=x,cy=y,ex=x,ey=y;
        bool closed=group==42 || group==43;
        if(group==42) rx=ry=p[0]&0x1fff;
        else if(group==43) {
            if(!p[0] || !p[1]) { fail("HD63484: zero ellipse coefficient"); break; }
            rx=p[2]; ry=rx*std::sqrt(double(p[1])/p[0]);
        } else {
            unsigned off=group==47 ? 2 : 0;
            cx=int16_t(x+int16_t(p[off])); cy=int16_t(y+int16_t(p[off+1]));
            ex=int16_t(x+int16_t(p[off+2])); ey=int16_t(y+int16_t(p[off+3]));
            double a=group==47?p[0]:1, b=group==47?p[1]:1;
            if(a==0 || b==0) { fail("HD63484: zero arc coefficient"); break; }
            double r=std::sqrt(double(x-cx)*(x-cx)/a+double(y-cy)*(y-cy)/b);
            rx=r*std::sqrt(a); ry=r*std::sqrt(b);
            if(rx==0 || ry==0) { fail("HD63484: zero arc radius"); break; }
            start=std::atan2((y-cy)/ry,(x-cx)/rx);
            double end=std::atan2((ey-cy)/ry,(ex-cx)/rx);
            sweep=end-start;
            if(op&0x100) { if(sweep>=0) sweep-=tau; }
            else if(sweep<=0) sweep+=tau;
        }
        curve(op,cx,cy,rx,ry,start,sweep,closed,ex,ey);
        if(!drawingStopped) position(ex,ey);
        break;
    }
    case 49: {
        int dx=int16_t(p[0]),dy=int16_t(p[1]);
        int sx=dx<0?-1:1,sy=dy<0?-1:1;
        for(int j=0;j<=std::abs(dy) && !drawingStopped;++j)
            for(int i=0;i<=std::abs(dx) && !drawingStopped;++i)
                patterned(op,x+i*sx,y+j*sy,i,j);
        if(!drawingStopped) position(x,y+dy+sy);
        break;
    }
    case 50: paint(op); break;
    case 51: patterned(op,x,y,0,0); break;
    default:
        if(group>=52 && group<=55) {
            if(op&0xf00) { fail("HD63484: unsupported PTN scan direction"); break; }
            unsigned w=(p[0]&255)+1,h=(p[0]>>8)+1;
            for(unsigned j=0;j<h && !drawingStopped;++j)
                for(unsigned i=0;i<w && !drawingStopped;++i) patterned(op,x+i,y+j,i,j);
            if(!drawingStopped) position(x,y+h);
        } else {
            unsigned direction=(op>>8)&15;
            if((direction!=0 && direction!=3 && direction!=12) || (op&0x18)) { fail("HD63484: unsupported AGCPY direction/color mode"); break; }
            int dx=int16_t(p[2]),dy=int16_t(p[3]);
            int sx=dx<0?-1:1,sy=dy<0?-1:1,d=direction==3?-1:1;
            // S=1, DSD=100 scans columns in both source and destination.
            // Scan order matters for overlap; the minor-axis CP advances past
            // the rectangle (User's Manual AGCPY, tables C37-1/C37-2).
            if(direction==12) {
                for(int i=0;i<=std::abs(dx) && !drawingStopped;++i)
                    for(int j=0;j<=std::abs(dy) && !drawingStopped;++j)
                        plot(op,x+i,y+j,pixel(int16_t(p[0])+i*sx,int16_t(p[1])+j*sy));
                if(!drawingStopped) position(x+std::abs(dx)+1,y);
            } else {
                for(int j=0;j<=std::abs(dy) && !drawingStopped;++j)
                    for(int i=0;i<=std::abs(dx) && !drawingStopped;++i)
                        plot(op,x+i*d,y+j*d,pixel(int16_t(p[0])+i*sx,int16_t(p[1])+j*sy));
                if(!drawingStopped) position(x,y+d*(std::abs(dy)+1));
            }
        }
        break;
    }
    return !drawingStopped;
}
}
