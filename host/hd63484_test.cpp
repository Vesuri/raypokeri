// Entirely synthetic data. Assertions use physical memory, not the model's pixel helper.
#include "../src/board/Hd63484.h"
#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
using pokeri::Hd63484;
static void check(bool b,const char *s) { if(!b) throw std::runtime_error(s); }
struct Video : Hd63484 {
    void word(unsigned w) { write8(2,w>>8);write8(2,w); }
    void cmd(std::initializer_list<unsigned> words) { write8(0,0);for(auto w:words)word(w); }
    void pr(unsigned r,unsigned v) { cmd({0x800+r,v}); }
    void reg(unsigned r,unsigned v) { write8(0,r);write8(2,v>>8);write8(0,r+1);write8(2,v); }
    void move(int x,int y) { cmd({0x8000,unsigned(uint16_t(x)),unsigned(uint16_t(y))}); }
    int x() const { return int16_t(parameter[0x12]); }
    int y() const { return int16_t(parameter[0x13]); }
    unsigned dot(int x,int y) const {
        int w=x/4,r=x%4; if(r<0){--w;r+=4;}
        return (frame[(0x1000+w-y*16)&frameMask]>>(r*4))&15;
    }
    void set(int x,int y,unsigned v) {
        int w=x/4,r=x%4; if(r<0){--w;r+=4;}
        auto &d=frame[(0x1000+w-y*16)&frameMask];d=(d&~(15<<(r*4)))|(v<<(r*4));
    }
    void fresh() { std::fill(frame.begin(),frame.end(),0);move(0,0); }
    void ok() { check(!error,"unexpected drawing error"); }
    Video() {
        reg(2,0x0200);reg(0xc2,16);
        cmd({0x400,1,0});pr(0,0x3333);pr(1,0xcccc);pr(4,0xffff);
        cmd({0x1800,1,0}); // one zero bit: solid CL0
    }
};
static void pointersAndFill() {
    Video v;
    v.move(-2,3);v.cmd({0x8400,5,0xfffc});
    check(v.x()==3 && v.y()==-1,"AMOVE/RMOVE signed coordinates");
    check(v.parameter[0x10]==1 && v.parameter[0x11]==0x010c,"DP physical word and dot follow CP");
    v.cmd({0x400,1,4});v.cmd({0xcc00});
    check(v.frame[0x1000]==0x30,"ORG clears CP and selects physical dot offset");
    v.pr(0,0x4321);v.cmd({0xcc00});check(v.frame[0x1000]==0x20,"color word selected by physical dot including origin offset");
    v.pr(0,0x3333);v.move(-2,1);v.cmd({0xcc00});
    check(v.frame[0xfef]==0x3000,"negative X floors word address, positive Y subtracts MW");
    v.reg(0xca,20);v.cmd({0x400,0x4001,0});v.move(0,1);v.cmd({0xcc00});
    check(v.frame[0xfec]==3,"ORG DN selects the screen memory width");
    v.pr(0xc,0);v.pr(0xd,0x4000);v.pr(4,0); // CLR must ignore MASK and use RWP's DN
    v.cmd({0x5800,0x1234,0xffff,1});
    check(v.frame[0x400]==0x1234 && v.frame[0x3ff]==0x1234 && v.frame[0x3f0]==0x1234 && v.frame[0x3ef]==0x1234,"CLR inclusive negative-X rectangle");
    check(v.rwp==0x3e0 && v.parameter[0xd]==0x3e00,"CLR advances RWP one row past rectangle");
    v.cmd({0x400,1,0});v.pr(4,0);v.move(2,2);v.cmd({0xc400,2,0xffff});
    check(v.dot(2,2)==3 && v.dot(4,1)==3 && v.dot(5,1)==0 && v.x()==2 && v.y()==0,"RFRCT includes corners, negative Y, ignores MASK, advances CP");
    v.move(2,2);v.pr(0,0x5555);v.cmd({0xc401,0,0});check(v.dot(2,2)==7,"RFRCT OR mode");
    v.move(2,2);v.cmd({0xcc02});check(v.dot(2,2)==5,"DOT AND");
    v.cmd({0xcc03});check(v.dot(2,2)==0,"DOT XOR");
    v.ok();
}
static void linesAndPatterns() {
    Video v;
    v.cmd({0x8c00,4,2});
    check(v.dot(0,0)==3 && v.dot(1,1)==3 && v.dot(2,1)==3 && v.dot(3,2)==3 && !v.dot(4,2),"RLINE Bresenham tie and excluded endpoint");
    check(v.x()==4 && v.y()==2,"RLINE CP endpoint");
    v.fresh();v.cmd({0x9800,2,3,0,3,3});
    check(v.dot(2,0)==3 && v.dot(3,0)==3 && v.dot(3,2)==3 && !v.dot(3,3),"APLL vertices connect without drawing final endpoint");
    v.fresh();v.cmd({0x9c00,2,3,0,0xfffe,2});
    check(v.x()==1 && v.y()==2 && v.dot(3,0)==3 && v.dot(2,1)==3 && !v.dot(1,2),"RPLL offsets are relative to previous vertex");
    v.fresh();v.cmd({0x1802,2,0x0005,0x0002});
    check(v.pattern[2]==5 && v.pattern[3]==2 && !v.pattern[0],"WPTN starts at PRA and preserves other rows");
    v.pr(5,0x2000);v.pr(6,0x2000);v.pr(7,0x3020);
    v.cmd({0xd000,0x0102});
    check(v.dot(0,0)==12 && v.dot(1,0)==3 && v.dot(2,0)==12 && v.dot(0,1)==3 && v.dot(1,1)==12 && v.dot(2,1)==3,"PTN scans chosen subrectangle LSB-first");
    check(v.x()==0 && v.y()==2,"PTN CP advances by height");
    v.fresh();v.set(1,0,7);v.cmd({0xd008,2});
    check(v.dot(0,0)==12 && v.dot(1,0)==7 && v.dot(2,0)==12,"PTN COL1 leaves zero-pattern destination intact");
    v.fresh();v.pr(5,0x2010);v.pr(7,0x3021);v.cmd({0xd000,3});
    check(v.dot(0,0)==3 && v.dot(1,0)==3 && v.dot(2,0)==12 && v.dot(3,0)==12,"pattern PP offset and twofold zoom");
    v.fresh();v.pr(5,0x2000);v.pr(7,0x3020);v.cmd({0x9c00,2,2,0,0,2});
    check(v.dot(0,0)==12 && v.dot(1,0)==3 && v.dot(2,0)==12 && v.dot(2,1)==12,"polyline pattern continues across vertices and wraps");
    v.ok();
}
static void curves() {
    Video v;
    v.cmd({0xa900,2});
    unsigned count=0;
    for(int y=-3;y<=3;++y)for(int x=-3;x<=3;++x)count+=v.dot(x,y)!=0;
    check(count==12 && v.dot(2,0)==3 && v.dot(0,-2)==3 && !v.dot(1,1) && !v.dot(0,0),"CRCL narrow radius-two contour");
    check(v.x()==0 && v.y()==0,"CRCL restores center");
    v.fresh();v.cmd({0xad00,9,4,9});
    check(v.dot(9,0)==3 && v.dot(-9,0)==3 && v.dot(0,6)==3 && v.dot(0,-6)==3 && !v.dot(0,7),"ELPS a:b is squared-axis ratio; third parameter is X radius");
    check(v.x()==0 && v.y()==0,"ELPS restores center");
    v.fresh();v.move(4,0);v.cmd({0xb500,0xfffc,0,0xfffc,0xfffc});
    check(v.dot(4,0)==3 && v.dot(3,-3)==3 && !v.dot(0,-4) && !v.dot(3,3),"RARC clockwise quadrant excludes final endpoint");
    check(v.x()==0 && v.y()==-4,"RARC relative endpoint updates CP");
    v.fresh();v.move(6,0);v.cmd({0xbc00,9,4,0xfffa,0,0xfffa,4});
    check(v.dot(6,0)==3 && v.dot(4,3)==3 && !v.dot(0,4) && !v.dot(4,-3),"REARC coefficients and counterclockwise direction");
    check(v.x()==0 && v.y()==4,"REARC CP");
    v.fresh();v.move(6,0);v.cmd({0xbd00,9,4,0xfffa,0,0xfffa,0xfffc});
    check(v.dot(4,-3)==3 && !v.dot(0,-4) && !v.dot(4,3),"REARC clockwise direction");
    v.fresh();v.cmd({0xa903,2});
    check(v.dot(2,0)==3 && v.dot(1,2)==3,"curve pixels emitted only once under XOR");
    v.ok();
}
static void copyAndPaint() {
    Video v;
    for(int y=0;y<2;++y)for(int x=0;x<3;++x)v.set(x,y,1+x+3*y);
    v.move(8,4);v.cmd({0xe000,0,0,2,1});
    check(v.dot(8,4)==1 && v.dot(10,5)==6 && v.y()==6,"AGCPY positive destination, inclusive size, CP");
    v.move(10,8);v.cmd({0xe300,0,0,2,1});
    check(v.dot(10,8)==1 && v.dot(8,7)==6 && v.x()==10 && v.y()==6,"AGCPY negative destination, CP follows destination not source direction");
    v.pr(6,0xf0); // copy does not consult pattern RAM control
    v.move(1,0);v.cmd({0xe000,0,0,3,0});
    check(v.dot(1,0)==1 && v.dot(4,0)==1,"AGCPY overlapping copy preserves hardware scan order");
    v.pr(6,0);v.fresh();v.pr(3,0xeeee);
    for(int i=-3;i<=3;++i){v.set(i,-3,14);v.set(i,3,14);v.set(-3,i,14);v.set(3,i,14);}
    v.cmd({0xc800});
    for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x)check(v.dot(x,y)==3,"PAINT fills entire enclosed area");
    check(v.dot(3,0)==14 && !v.dot(4,0) && !(v.statusNow()&Hd63484::RFR),"PAINT preserves boundary and exterior, simple fill has no continuation");
    v.fresh();v.pr(3,0); // inverse-edge mode: fill zero pixels only
    for(int i=-2;i<=2;++i){v.set(i,-2,5);v.set(i,2,5);v.set(-2,i,5);v.set(2,i,5);}
    v.cmd({0xc900});check(v.dot(1,1)==3 && v.dot(2,1)==5 && !v.dot(3,1),"PAINT inverse edge");
    v.fresh();std::fill(v.frame.begin(),v.frame.end(),0xeeee);v.pr(3,0xeeee);
    for(int i=0;i<5;++i){v.set(i,0,0);v.set(0,i,0);}
    for(int i=1;i<4;++i)v.set(3,i,0);
    v.cmd({0xc800});
    check(v.dot(0,4)==3 && v.dot(3,3)==3 && v.dot(4,0)==3 && v.dot(2,1)==14,"PAINT reaches concave branches without crossing edges");
    v.ok();
}
static void guards() {
    Video v;v.cmd({0xcc40});check(v.error && (v.statusNow()&Hd63484::CER),"unsupported area mode must be loud");
    v.cmd({0x8400,0,0});check(v.statusNow()&Hd63484::CER,"CER persists until abort");
    Video a;a.cmd({0x8800,1,1});check(a.error && a.unexecuted==1,"unimplemented command must not silently succeed");
    Video p;p.pr(6,0xf0);p.cmd({0xcc00});check(p.error,"invalid pattern bounds guarded before modulo");
    Video invalid;invalid.cmd({0xaa00,2});check(invalid.error,"reserved curve opcode bit must be rejected");
    Video r;r.cmd({0x180f,2,1,2});check(r.error,"WPTN range overflow loud");
    Video e;e.cmd({0xad00,0,1,2});check(e.error,"zero ellipse coefficient guarded");
    Video c;c.cmd({0x5800,0xffff,0x7fff,0x7fff});check(c.error,"excessive transfer work is a loud stop");
    Video paint;std::fill(paint.frame.begin(),paint.frame.end(),0xeeee);paint.pr(3,0xeeee);
    for(int x=0;x<9;++x)paint.set(x,0,0);
    for(int x=0;x<9;x+=2)paint.set(x,1,0);
    paint.cmd({0xc800});check(paint.error && !(paint.statusNow()&Hd63484::RFR),"PAINT overflow stops instead of faking FIFO continuation");
}
int main() try {
    pointersAndFill();linesAndPatterns();curves();copyAndPaint();guards();
    puts("PASS: HD63484 synthetic drawing commands, packing, pointers, patterns, directions, logical modes, bounded paint and unsupported-mode guards");
    return 0;
} catch(const std::exception &e) { std::fprintf(stderr,"FAIL: %s\n",e.what());return 1; }
