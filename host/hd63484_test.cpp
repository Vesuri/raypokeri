// Entirely synthetic data. Assertions use physical memory, not the model's pixel helper.
#include "../src/board/Hd63484.h"
#include "../src/board/PlanarSurface.h"
#include "../src/board/WordMath.h"
#include <climits>
#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
using pokeri::Hd63484;
static void check(bool b,const char *s) { if(!b) throw std::runtime_error(s); }
static bool planarMode=false;
struct Video : Hd63484 {
    struct Planes: pokeri::PlanarSurface {
        bool patternTile(uint32_t first,unsigned stride,const pokeri::PatternTile &tile,unsigned op)override{
            if(!tile.valid() || !stride || (stride&15) || tile.width>stride ||
               first+(tile.height-1)*stride+tile.width>words*4)return false;
            uint16_t expanded[160];tile.expand(expanded);
            for(unsigned y=0;y<tile.height;++y)for(unsigned x=0;x<tile.width;++x){
                unsigned dot=tile.offset+x,index=y*2+(dot>>4),mask=0x8000>>(dot&15);
                if(!(expanded[index]&mask))continue;
                unsigned color=0;for(unsigned p=0;p<4;++p)if(expanded[(p+1)*32+index]&mask)color|=1<<p;
                unsigned pixel=first+y*stride+x;plot4(pixel>>2,(pixel&3)*4,color,op);
            }
            return true;
        }
        unsigned fills=0,completedFills=0;uint16_t fillColor=0;bool fillEnabled=false;
        bool fill(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t color,unsigned op)override{
            ++fills;fillColor=color;
            if(!fillEnabled || !width || !height || width>stride ||
               first+(height-1)*stride+width>words*4)return false;
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
                unsigned pixel=first+y*stride+x;
                plot4(pixel>>2,(pixel&3)*4,(color>>((pixel&3)*4))&15,op);
            }
            ++completedFills;return true;
        }
    } planes;
    std::vector<uint16_t> storage;
    void fillWords(uint16_t value){for(unsigned a=0;a<=frameMask;++a)writeWord(a,value);}
    void word(unsigned w) { write8(2,w>>8);write8(2,w); }
    void cmd(std::initializer_list<unsigned> words) { write8(0,0);for(auto w:words)word(w); }
    void pr(unsigned r,unsigned v) { cmd({0x800+r,v}); }
    void reg(unsigned r,unsigned v) { write8(0,r);write8(2,v>>8);write8(0,r+1);write8(2,v); }
    void move(int x,int y) { cmd({0x8000,unsigned(uint16_t(x)),unsigned(uint16_t(y))}); }
    int x() const { return int16_t(parameter[0x12]); }
    int y() const { return int16_t(parameter[0x13]); }
    unsigned dot(int x,int y) const {
        int w=x/4,r=x%4; if(r<0){--w;r+=4;}
        return (readWord((0x1000+w-y*16)&frameMask)>>(r*4))&15;
    }
    void set(int x,int y,unsigned v) {
        int w=x/4,r=x%4; if(r<0){--w;r+=4;}
        unsigned a=(0x1000+w-y*16)&frameMask;writeWord(a,(readWord(a)&~(15<<(r*4)))|(v<<(r*4)));
    }
    void fresh() { fillWords(0);move(0,0); }
    void ok() { check(!error,"unexpected drawing error"); }
    Video() {
        if(planarMode){storage.resize(frame.size());planes.attach(storage.data(),storage.size());surface=&planes;}
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
    check(v.readWord(0x1000)==0x30,"ORG clears CP and selects physical dot offset");
    v.pr(0,0x4321);v.cmd({0xcc00});check(v.readWord(0x1000)==0x20,"color word selected by physical dot including origin offset");
    v.pr(0,0x3333);v.move(-2,1);v.cmd({0xcc00});
    check(v.readWord(0xfef)==0x3000,"negative X floors word address, positive Y subtracts MW");
    v.reg(0xca,20);v.cmd({0x400,0x4001,0});v.move(0,1);v.cmd({0xcc00});
    check(v.readWord(0xfec)==3,"ORG DN selects the screen memory width");
    v.pr(0xc,0);v.pr(0xd,0x4000);v.pr(4,0); // CLR must ignore MASK and use RWP's DN
    v.cmd({0x5800,0x1234,0xffff,1});
    check(v.readWord(0x400)==0x1234 && v.readWord(0x3ff)==0x1234 && v.readWord(0x3f0)==0x1234 && v.readWord(0x3ef)==0x1234,"CLR inclusive negative-X rectangle");
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
static void curveOrder() {
    // Hand-enumerated radius-two contour, clockwise from the positive X axis.
    // A single moving pattern bit exposes duplicate points or phase reordering.
    const int points[][2]={{2,0},{2,-1},{1,-2},{0,-2},{-1,-2},{-2,-1},
                          {-2,0},{-2,1},{-1,2},{0,2},{1,2},{2,1}};
    Video v;v.pr(7,0x00f0);
    for(unsigned reverse=0;reverse<2;++reverse)for(unsigned phase=0;phase<12;++phase){
        v.fresh();v.cmd({0x1800,1,1u<<phase});v.cmd({reverse?0xa803u:0xa903u,2});
        for(unsigned i=0;i<12;++i)check(v.dot(points[i][0],reverse?-points[i][1]:points[i][1])==(i==phase?12:3),"curve angular order and unique pattern phase");
    }
    v.ok();
}
static void copyAndPaint() {
    Video v;
    for(int y=0;y<2;++y)for(int x=0;x<3;++x)v.set(x,y,1+x+3*y);
    v.move(8,4);v.cmd({0xe000,0,0,2,1});
    check(v.dot(8,4)==1 && v.dot(10,5)==6 && v.y()==6,"AGCPY positive destination, inclusive size, CP");
    v.move(10,8);v.cmd({0xe300,0,0,2,1});
    check(v.dot(10,8)==1 && v.dot(8,7)==6 && v.x()==10 && v.y()==6,"AGCPY negative destination, CP follows destination not source direction");
    v.move(8,12);v.cmd({0xec00,0,0,2,1});
    check(v.dot(8,12)==1 && v.dot(10,13)==6 && v.x()==11 && v.y()==12,"AGCPY vertical source/destination and CP");
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
    v.fresh();v.fillWords(0xeeee);v.pr(3,0xeeee);
    for(int i=0;i<5;++i){v.set(i,0,0);v.set(0,i,0);}
    for(int i=1;i<4;++i)v.set(3,i,0);
    v.cmd({0xc800});
    check(v.dot(0,4)==3 && v.dot(3,3)==3 && v.dot(4,0)==3 && v.dot(2,1)==14,"PAINT reaches concave branches without crossing edges");
    v.ok();
}
static void activePatternFill(){
    if(!planarMode)return;
    Video v;v.pr(5,0x2030);v.pr(6,0x2030);v.pr(7,0x3050); // rows 2..3, bits 3..5
    v.cmd({0x1800,4,0x1234,0xabcd,0xffc7,0xffc7});
    v.cmd({0xc400,15,3});check(v.planes.fills==1 && v.planes.fillColor==0x3333,"inactive pattern bits must not prevent CL0 fill");
    v.cmd({0x1802,2,0x0038,0x0038});v.cmd({0xc400,15,3});
    check(v.planes.fills==2 && v.planes.fillColor==0xcccc,"active all-one pattern selects CL1 fill");
    v.cmd({0x1802,1,0x0030});v.cmd({0xc400,15,3});
    check(v.planes.fills==2,"mixed active pattern must retain patterned drawing");
    v.ok();
}
static void patternedPaint(){
    for(unsigned mode=0;mode<4;++mode)for(unsigned col=0;col<3;++col){
        Video v;v.fillWords(0xeeee);v.pr(3,0xeeee);v.pr(7,0x00f0);
        v.cmd({0x1800,1,0xaaaa});
        for(int y=-2;y<=2;++y)for(int x=-5;x<=5;++x)v.set(x,y,5);
        v.move(0,0);v.cmd({0xc800|(col<<3)|mode});v.ok();
        for(int y=-2;y<=2;++y)for(int x=-5;x<=5;++x){
            bool bit=(0xaaaa>>(unsigned(x)&15))&1;unsigned expected=5,color=bit?12:3;
            if(!((col==1 && !bit)||(col==2 && bit))){
                if(mode==0)expected=color;else if(mode==1)expected|=color;
                else if(mode==2)expected&=color;else expected^=color;
            }
            check(v.dot(x,y)==expected,"patterned/logical flood fill visits pixels exactly once");
        }
        check(v.dot(-6,0)==14 && v.dot(0,3)==14,"patterned fill preserves edge");
    }
}
static void cachedPatterns(){
    for(unsigned offset=0;offset<16;++offset)for(unsigned col=0;col<3;++col)for(unsigned op=0;op<4;++op){
        Video v;v.fillWords(0x5555);v.pr(0,0x1234);v.pr(1,0x89ab);
        v.pr(5,0x3040);v.pr(6,0x2020);v.pr(7,0x8070);
        v.cmd({0x1800,16,0x1357,0xabcd,0x9249,0x8421,0x00ff,0xa55a,0x5555,0xaaaa,0x3333,0,0,0,0,0,0,0});
        const unsigned rows[]={0x1357,0xabcd,0x9249,0x8421,0x00ff,0xa55a,0x5555,0xaaaa,0x3333};
        v.move(offset,0);v.cmd({0xd000|(col<<3)|op,0x0d0e});v.ok();
        for(unsigned y=0;y<14;++y)for(unsigned x=0;x<15;++x){
            bool bit=(rows[2+(1+y)%7]>>(2+(2+x)%6))&1;unsigned expected=5;
            unsigned color=((bit?0x89ab:0x1234)>>(((offset+x)&3)*4))&15;
            if(!((col==1 && !bit)||(col==2 && bit))){
                if(op==0)expected=color;else if(op==1)expected|=color;else if(op==2)expected&=color;else expected^=color;
            }
            check(v.dot(offset+x,y)==expected,"cached tile orientation, wrap, phase, mask and logic");
        }
        check(v.dot(offset-1,0)==5 && v.dot(offset+15,0)==5,"cached tile edges");
    }
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
    Video paint;paint.fillWords(0xeeee);paint.pr(3,0xeeee);
    for(int x=0;x<9;++x)paint.set(x,0,0);
    for(int x=0;x<9;x+=2)paint.set(x,1,0);
    paint.cmd({0xc800});check(paint.error && !(paint.statusNow()&Hd63484::RFR),"PAINT overflow stops instead of faking FIFO continuation");
}
static void solidPaintRows(){
    planarMode=false;Video reference;
    planarMode=true;Video fast;fast.planes.fillEnabled=true;
    for(Video *v:{&reference,&fast}){v->fillWords(0xeeee);v->pr(0,0x1234);v->pr(1,0xabcd);v->pr(3,0xeeee);}
    for(unsigned width:{1u,15u,16u,17u,33u})for(int left:{-3,0,5})
    for(unsigned op=0;op<4;++op)for(unsigned col=0;col<3;++col)for(unsigned bit=0;bit<2;++bit){
        unsigned before=fast.planes.completedFills;
        for(Video *v:{&reference,&fast}){
            // Closed rectangle plus an internal hole: every row's physical
            // pixels and the model's final CP/DP must agree with scalar PAINT.
            for(int y=-4;y<=4;++y)for(int x=-5;x<=40;++x)v->set(x,y,14);
            for(int y=-2;y<=2;++y)for(unsigned x=0;x<width;++x)v->set(left+x,y,5);
            if(width>16)v->set(left+7,1,14);
            v->cmd({0x1800,1,bit});v->move(left,0);v->cmd({0xc800|(col<<3)|op});v->ok();
        }
        for(int y=-4;y<=4;++y)for(int x=-5;x<=40;++x)
            check(reference.dot(x,y)==fast.dot(x,y),"solid PAINT spans match scalar pixels, holes, masks and ROPs");
        check(reference.parameter==fast.parameter,"solid PAINT final CP/DP and parameters");
        bool opaque=!((col==1 && !bit)||(col==2 && bit));
        check((fast.planes.completedFills>before)==(width>=16 && opaque),"only sufficiently wide opaque PAINT spans use fill");
    }
}
static void packedPixelAddressing(){
    // Independent packed-word oracle, including negative coordinates, origin
    // subword offsets, all depths and COL transparency. Exercise the planar
    // backend too; non-4bpp modes deliberately use its packed bus interface.
    Video v;v.pr(0,0x4321);v.pr(1,0xb976);
    for(unsigned mode=0;mode<=4;++mode){
        unsigned depth=1u<<mode,perWord=16/depth,pixelMask=(1u<<depth)-1;
        v.reg(2,mode<<8);
        for(unsigned originDot=0;originDot<16;++originDot){
            v.cmd({0x400,1,originDot});
            for(int x:{-32768,-17,-1,0,1,17,32767})for(int y:{-3,2}){
                int dot=x+int(originDot/depth),word=dot/int(perWord),sub=dot%int(perWord);
                if(sub<0){sub+=perWord;--word;}
                unsigned address=unsigned(0x1000+word-y*16)&v.frameMask,shift=unsigned(sub)*depth;
                uint16_t mask=uint16_t(pixelMask<<shift);
                for(unsigned bit=0;bit<2;++bit){
                    v.cmd({0x1800,1,bit});
                    uint16_t color=bit?0xb976:0x4321;
                    for(unsigned op=0;op<4;++op)for(unsigned col=0;col<3;++col){
                        v.writeWord(address,0x5aa5);v.move(x,y);v.cmd({0xcc00|(col<<3)|op});
                        uint16_t expected=0x5aa5,src=color&mask;
                        if(!((col==1 && !bit)||(col==2 && bit))){
                            if(op==0)expected=(expected&~mask)|src;
                            else if(op==1)expected|=src;
                            else if(op==2)expected&=uint16_t(~mask|src);
                            else expected^=src;
                        }
                        check(v.readWord(address)==expected,"pixel depth/origin/signed coordinate/ROP/COL packed oracle");
                    }
                }
            }
        }
    }
    v.ok();
}
static void patternArithmetic(){
    const int values[]={INT_MIN,INT_MIN+1,-131073,-65537,-32768,-257,-17,-1,0,1,17,255,32767,65536,INT_MAX};
    for(int d=1;d<=256;++d)for(int n:values){
        int expected=n%d;if(expected<0)expected+=d;
        check(pokeri::patternRemainder(n,d)==expected,"pattern signed remainder boundary");
    }
    for(unsigned d=1;d<=16;++d)for(unsigned n=0;n<65536;++n)
        check(pokeri::wordQuotient(n,d)==n/d,"pattern zoom quotient");
}
int main() try {
    patternArithmetic();
    for(bool planar: {false,true}){planarMode=planar;pointersAndFill();linesAndPatterns();curves();curveOrder();copyAndPaint();activePatternFill();patternedPaint();cachedPatterns();packedPixelAddressing();guards();}
    solidPaintRows();
    puts("PASS: packed and planar HD63484 synthetic drawing commands, packing, pointers, patterns, directions, logical modes, bounded paint and unsupported-mode guards");
    return 0;
} catch(const std::exception &e) { std::fprintf(stderr,"FAIL: %s\n",e.what());return 1; }
