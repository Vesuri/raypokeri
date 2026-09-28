// Entirely synthetic data. Assertions use physical memory, not the model's pixel helper.
#include "../src/board/Hd63484.h"
#include "../src/board/PlanarSurface.h"
#include "../src/board/WordMath.h"
#include <climits>
#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <string>
using pokeri::Hd63484;
static void check(bool b,const char *s) { if(!b) throw std::runtime_error(s); }
static bool planarMode=false,interleavedMode=false;
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
        if(planarMode){storage.resize(pokeri::PlanarLayout::storageWords(frame.size(),interleavedMode));planes.attach(storage.data(),frame.size(),interleavedMode);surface=&planes;}
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
static void ellipseMidpoints() {
    // Independent oracle evaluates the implicit equation at each midpoint;
    // no incremental error recurrence or rounded-axis radius substitution.
    for(int a:{1,4,9,25,64,256})for(int b:{1,4,9,16})for(int rx:{1,2,4,7,11}){
        bool expected[97][33]={};
        int radius=b*rx*rx,y=0,x=0;
        while(a*(y+1)*(y+1)<=radius)++y;
        if(4*radius>=a*(2*y+1)*(2*y+1))++y;
        auto mark=[&](){for(int sx:{-1,1})for(int sy:{-1,1})expected[48+sy*y][16+sx*x]=true;};
        while(b*x<a*y){
            mark();
            if(4*b*(x+1)*(x+1)+a*(2*y-1)*(2*y-1)>=4*radius)--y;
            ++x;
        }
        while(y>=0){
            mark();
            if(b*(2*x+1)*(2*x+1)+4*a*(y-1)*(y-1)<=4*radius)++x;
            --y;
        }
        Video v;v.cmd({0xad00,unsigned(a),unsigned(b),unsigned(rx)});
        for(int py=-48;py<=48;++py)for(int px=-16;px<=16;++px)
            check(v.dot(px,py)==(expected[48+py][16+px]?3u:0u),"ellipse agrees with direct implicit midpoint decisions after axis rounding");
        check(v.x()==0 && v.y()==0,"ellipse midpoint correction preserves CP");v.ok();
    }
}
static void fullTurnArcs() {
    // Synthetic closed arc versus complete primitive, including XOR and
    // pattern phase. A second cached traversal must cancel every dot.
    for(unsigned ellipse=0;ellipse<2;++ellipse)for(unsigned reverse=0;reverse<2;++reverse){
        Video arc,whole;
        unsigned flags=(reverse<<8)|3,rx=ellipse?6:2;
        for(Video *v:{&arc,&whole}){v->pr(7,0xf0);v->cmd({0x1800,1,0xa55a});}
        if(ellipse)whole.cmd({0xac00|flags,9,4,rx});else whole.cmd({0xa800|flags,rx});
        auto draw=[&](){
            arc.move(rx,0);
            if(ellipse)arc.cmd({0xbc00|flags,9,4,unsigned(uint16_t(-int(rx))),0,0,0});
            else arc.cmd({0xb400|flags,unsigned(uint16_t(-int(rx))),0,0,0});
        };
        draw();
        for(unsigned a=0;a<=arc.frameMask;++a)check(arc.readWord(a)==whole.readWord(a),"full-turn arc retains start once and matches full primitive pattern phase");
        check(arc.x()==int(rx) && arc.y()==0,"full-turn arc retains original endpoint CP");
        draw();
        check(arc.curveCacheHits==1,"second full-turn arc uses cached contour");
        for(unsigned a=0;a<=arc.frameMask;++a)check(!arc.readWord(a),"cached full-turn XOR arc cancels every point exactly once");
        arc.ok();whole.ok();
    }
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
static void cachedCurves() {
    // A fresh device provides the uncached result for each changed drawing
    // context. The warm device reuses only geometry: colours, pattern phase,
    // depth, origin, translation and ROP must all remain live inputs.
    Video warm;warm.frameMask=0x3fff;
    for(unsigned shape=0;shape<8;++shape)for(unsigned reverse=0;reverse<2;++reverse){
        auto draw=[&](Video &v,unsigned mode,int cx,int cy){
            unsigned flags=(reverse<<8)|mode;
            auto word=[](int n){return unsigned(uint16_t(n));};
            if(shape<3){
                v.move(cx,cy);
                if(shape==0)v.cmd({0xa800|flags,2});
                else if(shape==1)v.cmd({0xac00|flags,9,4,9});
                else v.cmd({0xac00|flags,4,9,6}); // Same implicit radius, different coefficients.
            }else{
                int sx=shape==7?0:6,sy=shape==7?6:0;
                v.move(cx+sx,cy+sy);
                int ex=shape==4?6:0,ey=shape==5?0:shape==6?-4:4;
                if(shape==3)v.cmd({0xb400|flags,word(-sx),word(-sy),word(ex-sx),word(ey-sy)});
                else v.cmd({0xbc00|flags,9,4,word(-sx),word(-sy),word(ex-sx),word(ey-sy)});
            }
        };
        draw(warm,0,0,0);warm.ok();
        for(unsigned depth=0;depth<=4;++depth)for(unsigned rop=0;rop<4;++rop)for(unsigned col=0;col<3;++col){
            Video cold;cold.frameMask=warm.frameMask;
            for(Video *v:{&warm,&cold}){
                v->fillWords(0x5aa5);v->reg(2,depth<<8);v->reg(0xc2,31);
                v->cmd({0x400,1,depth*3});v->pr(0,0x1234);v->pr(1,0x89ab);
                v->pr(5,0x0031);v->pr(6,0x0020);v->pr(7,0x00a2);
                v->cmd({0x1800,1,0xa55a});
            }
            uint32_t hits=warm.curveCacheHits;
            draw(warm,rop|(col<<3),-11,7);draw(cold,rop|(col<<3),-11,7);
            warm.ok();cold.ok();
            check(warm.curveCacheHits==hits+1 && cold.curveCacheHits==0,"translated curve reuses outline, fresh reference constructs it");
            for(unsigned a=0;a<=warm.frameMask;++a)
                check(warm.readWord(a)==cold.readWord(a),"cached curve preserves complete packed/planar pixels under changed drawing context");
            check(warm.parameter==cold.parameter && warm.statusNow()==cold.statusNow(),"cached curve preserves CP/DP, pattern state and status");
        }
    }
    Video evicted;evicted.cmd({0xa903,2});
    for(unsigned radius=3;radius<=10;++radius)evicted.cmd({0xa903,radius});
    unsigned misses=evicted.curveCacheMisses;
    evicted.fresh();evicted.cmd({0xa903,2});
    check(evicted.curveCacheMisses==misses+1 && evicted.dot(2,0)==3,"evicted geometry recomputes its exact outline");
    Video large;large.cmd({0xa903,120});misses=large.curveCacheMisses;
    large.cmd({0xa903,120});large.ok();
    check(large.curveCacheMisses==misses+1 && large.curveCacheHits==0,"large outlines bypass the bounded cache");
    for(unsigned a=0;a<=large.frameMask;++a)check(!large.readWord(a),"uncached XOR contour visits identical pixels twice");
}
static void cpuAccessScopes(){
    struct Deferred: pokeri::PlanarSurface {
        struct Fill {uint32_t first;unsigned stride,width,height;uint16_t color;unsigned op;};
        std::vector<Fill> pending;
        unsigned acquired=0,drained=0;bool capable=true;
        bool fill(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t color,unsigned op)override{
            pending.push_back({first,stride,width,height,color,op});return true;
        }
        bool cpuAccess4(pokeri::CpuPlanes &out)override{
            ++acquired;if(!capable)return false;
            for(const auto &f:pending){
                ++drained;
                for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x){
                    uint32_t pixel=f.first+y*f.stride+x;
                    PlanarSurface::plot4(pixel>>2,(pixel&3)*4,(f.color>>((pixel&3)*4))&15,f.op);
                }
            }
            pending.clear();return PlanarSurface::cpuAccess4(out);
        }
    } surface;
    planarMode=false;Video packed;planarMode=true;Video planar;
    surface.attach(planar.storage.data(),planar.planes.words,interleavedMode);planar.surface=&surface;
    for(Video *v:{&packed,&planar}){
        v->pr(0,0x1234);v->cmd({0x9803,4,3,3,10,3,10,6,0,0xfff9});v->ok();
    }
    check(surface.acquired==2 && surface.drained==2 && surface.pending.empty(),"CPU lease synchronizes again after queued axis segments, never once per point");
    for(unsigned a=0;a<=packed.frameMask;++a)check(packed.readWord(a)==planar.readWord(a),"CPU/queued polygon ordering matches packed pixels");
    const auto before=planar.storage;
    planar.pr(7,0xf0);planar.cmd({0x1800,1,0xa55a});planar.move(0,0);planar.cmd({0xa903,7});
    check(surface.acquired==3,"patterned curve takes one CPU lease");
    surface.capable=false;planar.cmd({0xa903,7});
    check(surface.acquired==4 && planar.storage==before,"unsupported CPU lease retains virtual fallback and tests capability once");
}
static void smallCurveArithmetic(){
    planarMode=false;Video packed;planarMode=true;Video planar;
    packed.frameMask=planar.frameMask=0x3ff;
    for(unsigned a:{1u,4u,9u,64u,256u})for(unsigned b:{1u,4u,9u,64u,256u})
    for(unsigned r:{0u,1u,2u,7u,16u,63u,90u})for(unsigned reverse=0;reverse<2;++reverse){
        for(Video *v:{&packed,&planar}){
            v->fillWords(0x55aa);v->pr(7,0xf0);v->cmd({0x1800,1,0xa55a});v->move(-7,3);
            v->cmd({0xac03|(reverse<<8),a,b,r});v->ok();
        }
        for(unsigned w=0;w<=packed.frameMask;++w)
            check(packed.readWord(w)==planar.readWord(w),"bounded midpoint arithmetic and point order match wide packed oracle");
    }
}
static void stampedCurves(){
    planarMode=false;Video packed;planarMode=true;Video planar;
    packed.frameMask=planar.frameMask=0x3ff;
    for(unsigned shape=0;shape<4;++shape)for(unsigned pitch:{0u,4u,31u,64u})
    for(unsigned align=0;align<16;++align)for(unsigned mode=0;mode<12;++mode){
        for(Video *v:{&packed,&planar}){
            v->fillWords(0x5aa5);v->reg(0xc2,pitch);
            v->cmd({0x400,0,align&3});v->pr(0,0x1234);v->pr(1,0xabcd);
            v->cmd({0x1800,1,(align&1)?0xffffu:0u});
            int center=shape==3?32765:int(align)-8;
            v->move(center,shape==3?-32767:3);
            unsigned op=shape==1?0xac00:shape==2?0xb400:0xa800;
            op|=(mode&3)|((mode>>2)<<3)|((align&1)<<8);
            if(shape==1)v->cmd({op,9,4,9});
            else if(shape==2)v->cmd({op,0xfffa,0,0xfffa,6});
            else v->cmd({op,7});
            v->ok();
        }
        for(unsigned a=0;a<=packed.frameMask;++a)
            check(packed.readWord(a)==planar.readWord(a),"curve stamps preserve packed pixels: alignments, colour phases, wrap, aliasing, ROP and COL");
        check(packed.parameter==planar.parameter,"curve stamps preserve final CP/DP");
    }
}
static void singlePointPatterns(){
    const int points[][2]={{2,0},{2,-1},{1,-2},{0,-2},{-1,-2},{-2,-1},
                          {-2,0},{-2,1},{-1,2},{0,2},{1,2},{2,1}};
    Video v;v.pr(0,0x1234);v.pr(1,0x89ab);
    for(unsigned column:{0u,5u,15u})for(unsigned row:{0u,7u,15u})
    for(unsigned zoom:{0u,3u,15u})for(unsigned tail:{0u,1u})
    for(unsigned bit=0;bit<2;++bit)for(unsigned col=0;col<3;++col)for(unsigned op=0;op<4;++op){
        unsigned bounds=(row<<12)|(column<<4),count=tail?zoom:0;
        v.pr(6,bounds);v.pr(7,bounds|(zoom<<8)|zoom);v.pr(5,bounds|(count<<8)|count);
        v.cmd({0x1800|row,1,(0xa55au&~(1u<<column))|(bit<<column)});
        for(int y=-3;y<=3;++y)for(int x=-3;x<=3;++x)v.set(x,y,5);
        v.move(0,0);v.cmd({0xa900|(col<<3)|op,2});v.ok();
        for(int y=-3;y<=3;++y)for(int x=-3;x<=3;++x){
            bool drawn=false;for(const auto &p:points)drawn|=x==p[0] && y==p[1];
            unsigned expected=5,color=((bit?0x89ab:0x1234)>>((unsigned(x)&3)*4))&15;
            if(drawn && !((col==1 && !bit)||(col==2 && bit))){
                if(op==0)expected=color;else if(op==1)expected|=color;
                else if(op==2)expected&=color;else expected^=color;
            }
            check(v.dot(x,y)==expected,"one-point pattern: selected bit, zoom/count, physical colour nibble, transparency and ROP");
        }
        check(v.parameter[5]==(bounds|(count<<8)|count),"drawing retains programmed one-point pattern phase");
    }
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
static void repeatingSelectors(){
    Video v;v.pr(0,0x1234);v.pr(1,0xabcd);v.cmd({0x1803,1,0xa659});
    for(unsigned length:{1u,2u,4u,8u,16u})for(unsigned start:{0u,16-length})
    for(unsigned point=0;point<length;++point)for(unsigned zoom:{1u,4u,16u})
    for(unsigned op=0;op<4;++op)for(unsigned col=0;col<3;++col){
        v.pr(5,0x3000|((zoom-1)<<8)|((start+point)<<4));v.pr(6,0x3000|(start<<4));
        v.pr(7,0x3000|((zoom-1)<<8)|((start+length-1)<<4));
        for(unsigned a=0xfc0;a<0x1040;++a)v.writeWord(a,0x5555);
        v.move(-3,0);v.cmd({0xc400|op|(col<<3),31,2});v.ok();
        for(unsigned y=0;y<3;++y)for(unsigned x=0;x<32;++x){
            bool bit=(0xa659>>(start+(point+x)%length))&1;
            unsigned expected=5,shift=(unsigned(int(x)-3)&3)*4,color=((bit?0xabcd:0x1234)>>shift)&15;
            if(!((col==1 && !bit)||(col==2 && bit))){
                if(op==0)expected=color;else if(op==1)expected|=color;
                else if(op==2)expected&=color;else expected^=color;
            }
            check(v.dot(int(x)-3,y)==expected,"prepared repeating selector: phase/start/negative address/y zoom/ROP/COL");
        }
        check(v.dot(-4,0)==5 && v.dot(29,0)==5,"prepared selector rectangle edges");
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
static void paintWordMasks(){
    planarMode=false;Video reference;
    planarMode=true;Video fast;fast.planes.fillEnabled=true;
    // Every physical word alignment, origin subpixel and row alignment. The
    // packed model remains the scalar oracle; neither its reads nor fills use
    // the planar word operations being tested here.
    for(unsigned mw:{16u,19u})for(unsigned offset=0;offset<4;++offset)
    for(unsigned align=0;align<16;++align)for(unsigned mode=0;mode<2;++mode)
    for(unsigned op=0;op<4;++op)for(unsigned col=0;col<3;++col){
        for(Video *v:{&reference,&fast}){
            v->reg(0xc2,mw);v->cmd({0x400,1,offset*4});
            v->pr(0,0x1234);v->pr(1,0xabcd);v->pr(3,mode?0x5555:0xeeee);
            v->pr(5,0);v->pr(6,0);v->pr(7,0x30);v->cmd({0x1800,1,col?0xau:0u});
            // Build directly in packed coordinates, independently of pixelAddress.
            for(int y=-4;y<=4;++y)for(int x=-2;x<40;++x){
                int dot=x+int(offset),word=dot>=0?dot/4:-((-dot+3)/4);
                unsigned a=(0x1000+word-y*int(mw))&v->frameMask,shift=(unsigned(dot)&3)*4;
                bool inside=y>=-2 && y<=2 && x>=int(align) && x<int(align)+19;
                if(y==1 && x==int(align)+7)inside=false;
                unsigned color=inside?5:14;
                v->writeWord(a,(v->readWord(a)&~(15<<shift))|(color<<shift));
            }
            v->move(align,0);v->cmd({0xc800|(mode<<8)|(col<<3)|op});v->ok();
        }
        check(reference.parameter==fast.parameter,"word PAINT CP/DP/origin/pattern/COL/ROP");
        for(unsigned a=0xfa0;a<0x1060;++a)
            check(reference.readWord(a)==fast.readWord(a),"word PAINT all alignments, edge modes, holes and row phases");
    }
}
static void paintFailureEquality(){
    for(unsigned shape=0;shape<4;++shape){
        planarMode=false;Video reference;
        planarMode=true;Video fast;fast.planes.fillEnabled=true;
        for(Video *v:{&reference,&fast}){
            v->pr(0,0x1111);v->pr(1,0x2222);v->pr(3,0xeeee);
            if(shape==0){ // Fifth seed: identical partial fill and failure.
                v->fillWords(0xeeee);
                for(int x=0;x<9;++x)v->set(x,0,0);
                for(int x=0;x<9;x+=2)v->set(x,1,0);
            }else if(shape==1){ // Coordinate limit, not a silently wrapped run.
                v->move(32767,0);
            }else{
                // Large fill crosses the inline span capacity and then reaches
                // the exact four-million-work boundary (opaque / transparent).
                v->frameMask=0xfffff;v->reg(0xc2,2048);v->cmd({0x400,0x80,0});
                for(unsigned a=0;a<0x100000;++a)v->writeWord(a,0xeeee);
                for(unsigned row=0;row<256;++row)for(unsigned w=0;w<2047;++w)
                    v->writeWord(0x80000+row*2048+w,0x5555);
            }
            v->cmd({0xc800|(shape==3?8u:0u)});
            check(v->error,"PAINT diagnostic case must stop");
            check(std::string(v->error).find(shape==0?"seed stack":shape==1?"coordinate wrap":"work limit")!=std::string::npos,"PAINT diagnostic case reaches intended guard");
        }
        check(std::string(reference.error)==fast.error,"word PAINT exact failure reason");
        check(reference.parameter==fast.parameter,"word PAINT exact partial-failure CP/DP");
        for(unsigned a=0;a<=reference.frameMask;++a)
            check(reference.readWord(a)==fast.readWord(a),"word PAINT exact partial-failure VRAM");
    }
}
static void uniformLineLimit(){
    for(unsigned op:{0u,3u}){
        planarMode=false;Video reference;
        planarMode=true;Video fast;
        for(Video *v:{&reference,&fast}){
            v->write8(0,0);v->word(0x9800|op);v->word(70);
            for(unsigned n=0;n<70;++n){v->word(n&1?0x7fff:0x8000);v->word(n&1?0x7fff:0x8001);}
            check(v->error && std::string(v->error).find("work limit")!=std::string::npos,"long polyline reaches work limit");
        }
        check(reference.parameter==fast.parameter,"planar line partial failure CP/DP");
        for(unsigned a=0;a<=reference.frameMask;++a)
            check(reference.readWord(a)==fast.readWord(a),"planar line partial work-limit VRAM");
    }
}
static void rotatedCopyFallbacks(){
    const int cases[][6]={{0,0,40,24,16,16},{3,1,24,12,16,16}, // disjoint and overlapping
        {32760,1,40,24,16,16},{1,32760,40,24,16,16}, // source coordinate wrap
        {1,1,-32760,24,16,16},{1,1,40,-32760,16,16}, // destination wrap
        {1,1,40,24,-16,16},{1,1,40,24,16,-16}}; // reverse source axes
    planarMode=false;Video reference;planarMode=true;Video fast;
    for(const auto &c:cases)for(unsigned op=0;op<4;++op){
        for(Video *v:{&reference,&fast}){
            for(unsigned a=0;a<=v->frameMask;++a)v->writeWord(a,uint16_t(a*8461+0xa659));
            v->move(c[2],c[3]);v->cmd({0xe300|op,unsigned(uint16_t(c[0])),unsigned(uint16_t(c[1])),unsigned(uint16_t(c[4])),unsigned(uint16_t(c[5]))});v->ok();
        }
        check(reference.parameter==fast.parameter,"rotated copy final CP/DP");
        for(unsigned a=0;a<=reference.frameMask;++a)
            check(reference.readWord(a)==fast.readWord(a),"rotated copy coordinate-wrap/overlap/source-direction fallback");
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
static void interruptMasks(){
    unsigned cases=0;
    for(unsigned queued: {0u,1u,7u,8u,9u,64u})for(bool pending: {false,true}){
        Hd63484 v(false);
        for(unsigned n=0;n<queued;++n){v.write8(2,0x0c);v.write8(2,0);}
        if(pending){v.write8(2,0x08);v.write8(2,0);}
        check(!v.error,"IRQ fixture commands accepted");
        for(bool busy: {false,true}){
            v.presentationBusy=busy;
            for(unsigned status=0;status<256;++status){
                v.status=uint8_t(status);
                const uint8_t expected=uint8_t((status&0xd0) |
                    ((!pending && (status&0x20))?0x20:0) |
                    (busy?0:3) | (queued?4:0) | (queued>=8?8:0));
                check(v.statusNow()==expected,"IRQ independent status oracle");
                for(unsigned mask=0;mask<256;++mask){
                    v.control[3]=uint8_t(mask);
                    check(v.irq()==bool(expected&mask),"IRQ enabled-source equivalence");
                    ++cases;
                }
            }
        }
    }
    std::printf("PASS: %u IRQ source/mask combinations\n",cases);
}
int main(int argc,char **) try {
    interleavedMode=argc>1;
    interruptMasks();patternArithmetic();
    for(bool planar: {false,true}){planarMode=planar;pointersAndFill();linesAndPatterns();curves();ellipseMidpoints();fullTurnArcs();curveOrder();cachedCurves();singlePointPatterns();copyAndPaint();activePatternFill();patternedPaint();cachedPatterns();repeatingSelectors();packedPixelAddressing();guards();}
    cpuAccessScopes();smallCurveArithmetic();stampedCurves();solidPaintRows();paintWordMasks();paintFailureEquality();uniformLineLimit();rotatedCopyFallbacks();
    puts("PASS: packed and planar HD63484 synthetic drawing commands, packing, pointers, patterns, directions, logical modes, bounded paint and unsupported-mode guards");
    return 0;
} catch(const std::exception &e) { std::fprintf(stderr,"FAIL: %s\n",e.what());return 1; }
