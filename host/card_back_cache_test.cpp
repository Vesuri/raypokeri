// Local descriptor only. All backgrounds, interruptions and mutations are synthetic.
#include "../src/board/CardBackCache.h"
#include "../src/board/PlanarSurface.h"
#include "../amiga/generated/CardBackRecipe.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
using namespace pokeri;
static unsigned cases=0,hits=0;
static std::vector<uint16_t> logs[2];
static void check(bool okay,const char *message){if(!okay)throw std::runtime_error(message);}
static void log(unsigned which,const uint16_t *w,unsigned n,bool done){logs[which].push_back(n);logs[which].push_back(done);logs[which].insert(logs[which].end(),w,w+n);}
// Optional rectangle acceleration has different diagnostic work counts. Keep
// a packed oracle with the same fill contract to verify that prefix too.
struct PackedFill : Surface {
    Hd63484 *video=nullptr;
    uint16_t readWord(uint32_t a)const override{return video->frame[a];}
    void writeWord(uint32_t a,uint16_t w)override{video->frame[a]=w;}
    uint16_t pixel4(uint32_t a,unsigned shift)const override{return (readWord(a)>>shift)&15;}
    void plot4(uint32_t a,unsigned shift,unsigned color,unsigned op)override{
        unsigned d=pixel4(a,shift);if(op==1)color|=d;else if(op==2)color&=d;else if(op==3)color^=d;
        writeWord(a,(readWord(a)&~(15<<shift))|((color&15)<<shift));
    }
    bool fill(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t color,unsigned op)override{
        if(op || stride!=608 || first+(height-1)*stride+width>0x100000)return false;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){unsigned p=first+y*stride+x,shift=(p&3)*4;
            video->frame[p>>2]=(video->frame[p>>2]&~(15<<shift))|(color&(15<<shift));}
        return true;
    }
};
struct TestSurface : PlanarSurface {
    bool rectangles=false;
    bool fill(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t color,unsigned op)override{
        if(!rectangles || op || stride!=608 || first+(height-1)*stride+width>0x100000)return false;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){unsigned p=first+y*stride+x;
            plot4(p>>2,(p&3)*4,(color>>((p&3)*4))&15,0);}
        return true;
    }
};
struct Fixture {
    Hd63484 reference,actual;
    TestSurface surface;
    PackedFill packed;
    std::vector<uint16_t> storage,image,mask;
    CardBackCache cache;
    std::vector<uint16_t> stream;
    Fixture(unsigned bg=0,int x=0,int y=126,bool rows=true,bool accelerated=false):
        storage(PlanarLayout::storageWords(0x40000,rows)),image(CardBackCache::BitmapWords),mask(CardBackCache::BitmapWords){
        surface.attach(storage.data(),0x40000,rows);actual.surface=&surface;surface.rectangles=accelerated;
        packed.video=&reference;if(accelerated)reference.surface=&packed;
        for(auto *v:{&reference,&actual}){
            auto c=card_recipe::context;v->origin=c[0];v->frameMask=c[1];v->rwp=c[2];v->status=c[3];
            for(unsigned i=0;i<32;++i)v->parameter[i]=c[4+i];
            for(unsigned i=0;i<16;++i)v->pattern[i]=c[36+i];
            for(unsigned i=0;i<256;++i)v->control[i]=c[52+i];
        }
        check(cache.prepare({card_recipe::words,card_recipe::offsets,card_recipe::context},image.data(),mask.data()),cache.error?cache.error:"prepare");
        check(cache.coverage==8652 && cache.guardCount==68,"coverage/dependency proof changed");
        cache.attach(actual,accelerated);
        uint32_t rng=17;
        for(unsigned a=0;a<0x40000;++a){rng=rng*1664525u+1013904223u;uint16_t value=bg<16?bg*0x1111u:uint16_t(rng>>16);
            reference.frame[a]=value;surface.writeWord(a,value);}
        // Random admitted backgrounds retain random colours in every
        // untouched region; only the derived input predicates are constrained.
        if(bg==17)for(unsigned g=0;g<cache.guardCount;++g){
            unsigned row=cache.guards[g].pixel/88,col=cache.guards[g].pixel%88;
            int dot=x+int(col),word=dot>=0?dot/4:-int((unsigned(-dot)+3)/4);
            unsigned a=((reference.origin>>4)+word-(y+row)*152)&reference.frameMask,shift=(unsigned(dot)&3)*4;
            uint16_t value=reference.frame[a]&~(15<<shift);reference.frame[a]=value;surface.writeWord(a,value);
        }
        stream.assign(card_recipe::words,card_recipe::words+CardBackCache::Words);
        for(unsigned n=0;n<79;++n){unsigned i=card_recipe::offsets[n];
            if(stream[i]==0x8000){stream[i+1]+=x;stream[i+2]+=y;}}
        logs[0].clear();logs[1].clear();
        reference.commandLog=[](const uint16_t*w,unsigned n,bool d){log(0,w,n,d);};
        actual.commandLog=[](const uint16_t*w,unsigned n,bool d){log(1,w,n,d);};
    }
    void semantic(){
        check(reference.parameter==actual.parameter,"parameter prefix differs");
        check(reference.control==actual.control && reference.pattern==actual.pattern,"control/pattern differs");
        check(reference.rwp==actual.rwp && reference.origin==actual.origin,"address state differs");
        check(reference.statusNow()==actual.statusNow(),"status differs");
        check(reference.commands==actual.commands && logs[0]==logs[1],"command counters/logs differ");
        check(reference.drawingWorkCount()==actual.drawingWorkCount(),"work count differs");
        check(reference.drawingFailed()==actual.drawingFailed(),"drawing stop differs");
        check((!reference.error && !actual.error) || (reference.error && actual.error && !std::strcmp(reference.error,actual.error)),"fault differs");
    }
    void word(uint16_t w){reference.writeFifoWord(w);actual.writeFifoWord(w);semantic();}
    void command(unsigned c){for(unsigned i=card_recipe::offsets[c];i<card_recipe::offsets[c+1] && !reference.error;++i)word(stream[i]);}
    void snapshot(bool restore=false){
        State a,b;reference.state(a);actual.state(b);check(a.bytes==b.bytes,"snapshot state/protocol/canonical VRAM differs");
        if(restore){State ra(a.bytes),rb(b.bytes);reference.state(ra);actual.state(rb);}
    }
    void finish(bool serialize=false){
        actual.flushCard();semantic();
        for(unsigned a=0;a<=reference.frameMask;++a)if(reference.frame[a]!=surface.readWord(a)){
            std::fprintf(stderr,"case %u: VRAM word %x ref=%04x cache=%04x hits=%u\n",cases,a,reference.frame[a],surface.readWord(a),cache.hits);
            throw std::runtime_error("VRAM differs");
        }
        if(serialize && !reference.error)snapshot();
        hits+=cache.hits;++cases;
    }
    void run(){for(unsigned c=0;c<79 && !reference.error;++c)command(c);}
};
int main()try{
    for(bool rows:{false,true})for(unsigned align=0;align<16;++align)for(unsigned bg=0;bg<18;++bg){
        Fixture f(bg,align,126,rows);f.run();f.finish();
        if(bg<16)check(f.cache.hits==unsigned(bg!=1 && bg!=15),"solid background guard admission differs");
        if(bg==17)check(f.cache.hits==1,"guarded random background declined");
    }
    for(int x:{-296,0,239,240,241,32750})for(int y:{-1200,-1000,0,126}){
        Fixture f(17,x,y);f.run();f.finish();
    }
    // Every word includes every opcode, fixed parameter and polyline vertex.
    for(unsigned mutate=0;mutate<CardBackCache::Words;++mutate){
        Fixture f;f.stream[mutate]^=1;
        for(uint16_t w:f.stream){if(f.reference.error)break;f.word(w);}
        f.finish(true);check(!f.cache.hits,"mutated recipe admitted");
    }
    // Every observation boundary, including a partial byte and command.
    for(unsigned boundary=0;boundary<=79;++boundary)for(unsigned kind=0;kind<10;++kind){
        Fixture f;
        for(unsigned c=0;c<boundary;++c)f.command(c);
        if(kind==0)check(f.reference.read8(0)==f.actual.read8(0),"status observation differs");
        if(kind==1)check(f.reference.readWord(17)==f.actual.readWord(17),"VRAM observation differs");
        if(kind==2){f.reference.writeWord(17,0x369c);f.actual.writeWord(17,0x369c);}
        if(kind==3){f.word(0x0c12);check(f.reference.read8(2)==f.actual.read8(2),"RPR high differs");check(f.reference.read8(2)==f.actual.read8(2),"RPR low differs");}
        if(kind==4){for(auto *v:{&f.reference,&f.actual}){v->write8(0,2);v->write8(2,v->control[2]|0x80);v->write8(0,0);}}
        if(kind==5)f.snapshot(true);
        if(kind==6){f.word(0x0802);f.word(0x9999);}
        if(kind==7){for(auto *v:{&f.reference,&f.actual})v->write8(2,0x80);f.snapshot(true);
            for(auto *v:{&f.reference,&f.actual})v->write8(2,0);f.word(0);f.snapshot(true);f.word(126);}
        if(kind==9){for(auto *v:{&f.reference,&f.actual}){v->write8(0,3);v->write8(2,0x81);v->write8(0,0);}}
        if(kind!=8)for(unsigned c=boundary;c<79 && !f.reference.error;++c)f.command(c);
        f.finish(true);
        if(kind==0 || kind==9)check(f.cache.hits==1,"status/interrupt-enable access spuriously flushes a match");
    }
    for(unsigned boundary=0;boundary<=79;++boundary){
        Fixture f(0,3,126,true,true);
        for(unsigned c=0;c<boundary;++c)f.command(c);
        f.snapshot();
        for(unsigned c=boundary;c<79;++c)f.command(c);
        f.finish(true);
    }
    for(unsigned field=0;field<20;++field){
        Fixture f;
        for(auto *v:{&f.reference,&f.actual}){
            if(field<8)v->pattern[0]^=1<<field;
            else if(field<14){unsigned p[]={2,4,8,9,10,11};v->parameter[p[field-8]]^=1;}
            else if(field==14)v->origin^=1;
            else if(field==15)v->frameMask=0x1ffff;
            else if(field==16)v->control[0xcb]^=1;
            else if(field==17)v->control[2]=1;
            else if(field==18)v->wptnCountsBytes=true;
            else v->status|=Hd63484::CER;
        }
        f.run();f.finish();check(!f.cache.hits,"changed entry context admitted");
    }
    std::printf("PASS: card cache %u differential cases / %u hits; pixels, prefix state/work, logs, mutations, observations and snapshots\n",cases,hits);
    return 0;
}catch(const std::exception &e){std::fprintf(stderr,"FAIL case %u: %s\n",cases,e.what());return 1;}
