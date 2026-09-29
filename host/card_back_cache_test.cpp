// Local descriptor only. All backgrounds, interruptions and mutations are synthetic.
#include "../src/board/CardBackCache.h"
#include "cached_raster_reference.h"
#include "cached_batch_reference.h"
#include "../src/native/CachedBatch.h"
#include "../src/board/PlanarSurface.h"
#include "../amiga/generated/CardBackRecipe.h"
#ifdef POKERI_CARD_PREPARED
#include "../amiga/generated/CardBackPrepared.h"
#endif
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
using namespace pokeri;
static unsigned cases=0,hits=0,whiteHits=0,grantHits=0;
static bool grants=false,controls=false,absolute=false;
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
    bool rectangles=false,planeReads=true;
    mutable unsigned planeAttempts=0;
    bool readPlanes4(uint32_t a,uint16_t *out)const override{
        ++planeAttempts;return planeReads && PlanarSurface::readPlanes4(a,out);
    }
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
    CardBackCache::RasterGrant grant;
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
#ifdef POKERI_CARD_PREPARED
        check(cache.installPrepared({card_recipe::words,card_recipe::offsets,card_recipe::context},card_prepared::data,image.data(),mask.data()),cache.error?cache.error:"install prepared");
#else
        check(cache.prepare({card_recipe::words,card_recipe::offsets,card_recipe::context},image.data(),mask.data()),cache.error?cache.error:"prepare");
#endif
        check(cache.whiteReady,"shared white prefix proof failed");
        check(cache.coverage==8652 && cache.guardCount==68,"coverage/dependency proof changed");
        cache.attach(actual,accelerated);
        uint32_t rng=17;
        for(unsigned a=0;a<0x40000;++a){rng=rng*1664525u+1013904223u;uint16_t value=bg<16?bg*0x1111u:uint16_t(rng>>16);
            reference.frame[a]=value;surface.writeWord(a,value);}
        // Random admitted backgrounds retain random colours in every
        // untouched region; only the derived input predicates are constrained.
        if(bg==17)for(unsigned g=0;g<cache.guardCount;++g){
            unsigned row=99-cache.guards[g].offset/608,col=cache.guards[g].offset%608;
            int dot=x+int(col),word=dot>=0?dot/4:-int((unsigned(-dot)+3)/4);
            unsigned a=((reference.origin>>4)+word-(y+row)*152)&reference.frameMask,shift=(unsigned(dot)&3)*4;
            uint16_t value=reference.frame[a]&~(15<<shift);reference.frame[a]=value;surface.writeWord(a,value);
        }
        stream.assign(card_recipe::words,card_recipe::words+CardBackCache::Words);
        for(unsigned n=0;n<79;++n){unsigned i=card_recipe::offsets[n];
            if(stream[i]==0x8000){stream[i+1]+=x;stream[i+2]+=y;}}
        logs[0].clear();logs[1].clear();
        reference.commandLog=[](const uint16_t*w,unsigned n,bool d){log(0,w,n,d);};
        if(!grants)actual.commandLog=[](const uint16_t*w,unsigned n,bool d){log(1,w,n,d);};
    }
    void semantic(){
        check(reference.parameter==actual.parameter,"parameter prefix differs");
        check(reference.control==actual.control && reference.pattern==actual.pattern,"control/pattern differs");
        check(reference.rwp==actual.rwp && reference.origin==actual.origin,"address state differs");
        check(reference.statusNow()==actual.statusNow(),"status differs");
        check(reference.commands==actual.commands && (grants || logs[0]==logs[1]),"command counters/logs differ");
        check(reference.drawingWorkCount()==actual.drawingWorkCount(),"work count differs");
        check(reference.drawingFailed()==actual.drawingFailed(),"drawing stop differs");
        check((!reference.error && !actual.error) || (reference.error && actual.error && !std::strcmp(reference.error,actual.error)),"fault differs");
    }
    void word(uint16_t w){
        reference.writeFifoWord(w);
        if(grants && cache.rasterGrant(actual,grant,controls,absolute) && applyRaster(grant,w))++grantHits;
        else actual.writeFifoWord(w);
        semantic();
    }
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
        hits+=cache.hits;whiteHits+=cache.whiteHits;++cases;
    }
    void run(){for(unsigned c=0;c<79 && !reference.error;++c)command(c);}
};
// Independent address check across signed coordinates, origin nibble offsets,
// frame seams and every pixel of a card, not just this recipe's 68 guards.
static void guardAddressCheck(){
    Hd63484 v;v.frameMask=0x3ffff;v.control[2]=2;
    for(unsigned slot=0;slot<4;++slot){v.control[0xc2+8*slot]=0;v.control[0xc3+8*slot]=152;}
    // Exercise the public AMOVE command and read its resulting DP registers.
    // This tests the actual address calculation without exposing private APIs.
    auto pixel=[&](int x,int y){
        v.writeFifoWord(0x8000);v.writeFifoWord(uint16_t(x));v.writeFifoWord(uint16_t(y));
        check(!v.error,"address probe AMOVE failed");
        uint32_t a=((uint32_t(v.parameter[0x10]&255)<<12)|(v.parameter[0x11]>>4))&v.frameMask;
        return a*4+((v.parameter[0x11]&15)>>2);
    };
    unsigned checked=0;
    for(uint32_t origin:{0u,1u,3u,4u,7u,8u,15u,0x80000u,0xc0080000u})
    for(int x:{-32768,-297,-296,-87,-17,-4,-3,-1,0,1,15,16,87,240,32680})
    for(int y:{-32768,-1200,-1000,-99,-1,0,126,1000,32668}){
        v.origin=origin;uint32_t first=pixel(x,y+99);
        if(first+99*608+88>0x100000)continue;
        for(unsigned row=0;row<100;++row)for(unsigned col=0;col<88;++col){
            check(pixel(x+int(col),y+int(row))==first+(99-row)*608+col,"precomputed guard address differs");++checked;
        }
    }
    check(checked==7594400,"guard address coverage changed");
}
template<class Batch> static void batchCheck(){
    grants=controls=absolute=true;
    unsigned borrowed=0,cuts=0;
    for(bool rectangles:{false,true})for(int x:{-3,0,15,240})
    for(unsigned cut=0;cut<=CardBackCache::Words;++cut){
        Fixture f(17,x,126,true,rectangles);Batch batch;
        // Untouched state must not be replaced by startup snapshots.
        f.reference.parameter[24]=f.actual.parameter[24]=0xa55a;
        for(unsigned group=0;group<64;++group)
            f.reference.commands[group]=f.actual.commands[group]=~Hd63484::CommandCount(0);
        for(unsigned i=0;i<CardBackCache::Words;++i){
            if(i==cut){batch.materialize();f.semantic();f.snapshot();++cuts;}
            f.reference.writeFifoWord(f.stream[i]);
            if(!batch.borrowed() && f.cache.rasterGrant(f.actual,f.grant,true,true))batch.begin(f.grant);
            if(batch.accept(f.stream[i]))++borrowed;
            else {batch.materialize();f.actual.writeFifoWord(f.stream[i]);f.semantic();}
        }
        batch.materialize();f.semantic();if(cut==CardBackCache::Words){f.snapshot();++cuts;}f.finish(true);
    }
    // Repeated materializations without pixel observation keep the admitted
    // recipe live. Interrupts can split at partial and complete commands.
    for(bool rectangles:{false,true})for(unsigned stride:{1u,2u,3u,7u,16u,31u,127u}){
        Fixture f(17,15,126,true,rectangles);Batch batch;
        for(unsigned i=0;i<CardBackCache::Words;++i){
            if(!(i%stride)){batch.materialize();f.semantic();}
            f.reference.writeFifoWord(f.stream[i]);
            if(!batch.borrowed() && f.cache.rasterGrant(f.actual,f.grant,true,true))batch.begin(f.grant);
            if(!batch.accept(f.stream[i])){batch.materialize();f.actual.writeFifoWord(f.stream[i]);f.semantic();}
        }
        batch.materialize();f.semantic();f.finish(true);
    }
    // Reuse one translated recipe across owners, positions and option changes.
    // Borrow options can change after every interruption without stale bounds.
    {
        Batch batch;
        for(bool rectangles:{false,true})for(int x:{15,-3,240,0,15}){
            Fixture f(17,x,126,true,rectangles);
            for(unsigned i=0;i<CardBackCache::Words;++i){
                if(!(i%5)){batch.materialize();f.semantic();}
                f.reference.writeFifoWord(f.stream[i]);
                bool control=(i/5)&1,absolute=(i/10)&1;
                if(!batch.borrowed() && f.cache.rasterGrant(f.actual,f.grant,control,absolute))batch.begin(f.grant);
                if(!batch.accept(f.stream[i])){batch.materialize();f.actual.writeFifoWord(f.stream[i]);f.semantic();}
            }
            batch.materialize();f.semantic();f.finish(true);
        }
    }
    // Every possible mismatch must materialize only earlier accepted words.
    for(unsigned mutation=0;mutation<CardBackCache::Words;++mutation){
        Fixture f(17,16,126);Batch batch;
        for(unsigned i=0;i<CardBackCache::Words && !f.reference.error;++i){
            uint16_t value=f.stream[i]^(i==mutation?1:0);
            f.reference.writeFifoWord(value);
            if(!batch.borrowed() && f.cache.rasterGrant(f.actual,f.grant,true,true))batch.begin(f.grant);
            if(!batch.accept(value)){batch.materialize();f.actual.writeFifoWord(value);f.semantic();}
        }
        batch.materialize();f.semantic();f.finish(false);
    }
    check(borrowed && cuts,"batch path unused");
    std::printf("PASS: batch prefix specification %u cuts / %u borrowed words; full continuation, mismatches and snapshots\n",cuts,borrowed);
}
static void partialBatchCheck(){
    grants=controls=absolute=true;unsigned partial=0,rejected=0,continued=0;
    for(int x:{-3,15,240})for(unsigned cut=0;cut<CardBackCache::Words;++cut){
        Fixture f(17,x,126,true,true);pokeri::CachedBatch batch;
        for(unsigned i=0;i<cut;++i){f.reference.writeFifoWord(f.stream[i]);f.actual.writeFifoWord(f.stream[i]);}
        if(f.cache.rasterGrant(f.actual,f.grant,true,true)){
            unsigned count=*f.grant.pendingCount;
            int savedLength=*f.grant.pendingLength;uint8_t savedHigh=*f.grant.writeHigh;
            bool admitted=batch.begin(f.grant);if(admitted && count)++partial;
            // Snapshotting flushes recognition, so retain the live grant for
            // refusal/continuation tests and compare protocol fields directly.
            // The compiled CPU proof also checks every byte on an empty borrow.
            batch.materialize();f.semantic();
            check(*f.grant.pendingCount==count && *f.grant.pendingLength==savedLength && *f.grant.writeHigh==savedHigh,"empty partial borrow changed protocol");
            if(admitted && count){
                for(unsigned n=0;n<count;++n){
                    f.grant.pending[n]^=1;check(!batch.begin(f.grant),"mutated accepted prefix admitted");
                    check(!batch.borrowed(),"rejected prefix left a borrow");f.grant.pending[n]^=1;++rejected;
                    check(batch.begin(f.grant),"restored valid prefix not admitted");batch.materialize();
                }
                int length=*f.grant.pendingLength;*f.grant.pendingLength=0;
                check(!batch.begin(f.grant),"wrong partial length admitted");*f.grant.pendingLength=length;++rejected;
                *f.grant.pendingCount=64;check(!batch.begin(f.grant),"oversized partial admitted");*f.grant.pendingCount=count;++rejected;
                check(batch.begin(f.grant),"restored valid partial state not admitted");batch.materialize();
            }
        }
        for(unsigned i=cut;i<CardBackCache::Words;++i){
            f.reference.writeFifoWord(f.stream[i]);
            if(!batch.borrowed() && f.cache.rasterGrant(f.actual,f.grant,true,true))batch.begin(f.grant);
            if(batch.accept(f.stream[i]))++continued;
            else {batch.materialize();f.actual.writeFifoWord(f.stream[i]);}
        }
        batch.materialize();f.semantic();f.finish(true);
    }
    check(partial>100 && rejected>partial && continued>1000,"partial admission proof unused");
    std::printf("PASS: %u partial re-admissions, %u prefix/length/count refusals, %u continued words; no-word identity and full continuation\n",partial,rejected,continued);
}
#ifdef POKERI_CARD_PREPARED
static void preparedCheck(){
    const CardBackCache::Recipe recipe={card_recipe::words,card_recipe::offsets,card_recipe::context};
    std::vector<uint16_t> image(CardBackCache::BitmapWords),mask(image.size()),loaded(image.size()),loadedMask(image.size());
    CardBackCache reference,cache;
    check(reference.prepare(recipe,image.data(),mask.data()),"reference preparation");
    check(cache.installPrepared(recipe,card_prepared::data,loaded.data(),loadedMask.data()),"prepared install");
    check(image==loaded && mask==loadedMask,"prepared pixels/mask differ");
    check(reference.guardCount==cache.guardCount && reference.coverage==cache.coverage && reference.whiteReady==cache.whiteReady,"prepared metadata differs");
    for(unsigned i=0;i<cache.guardCount;++i)check(reference.guards[i].offset==cache.guards[i].offset && reference.guards[i].allowed==cache.guards[i].allowed,"prepared guard differs");
    for(unsigned i=0;i<CardBackCache::Commands;++i){auto a=reference.progress[i],b=cache.progress[i];
        check(a.x==b.x && a.y==b.y && a.scalarWork==b.scalarWork && a.rectangleWork==b.rectangleWork,"prepared progress differs");}
    for(unsigned mutation=0;mutation<12;++mutation){
        auto data=card_prepared::data;
        std::vector<uint16_t> words(data.source.words,data.source.words+CardBackCache::Words),offsets(data.source.offsets,data.source.offsets+CardBackCache::Commands+1);
        std::vector<uint32_t> context(data.source.context,data.source.context+308);
        std::vector<CardBackCache::Guard> guards(data.guards,data.guards+data.guardCount);
        std::vector<CardBackCache::Progress> progress(data.progress,data.progress+CardBackCache::Commands);
        std::vector<uint16_t> badImage=image,badMask=mask;
        switch(mutation){
        case 0:++data.version;break;
        case 1:data.guardCount=CardBackCache::MaxGuards+1;break;
        case 2:data.coverage=8801;break;
        case 3:data.image=nullptr;break;
        case 4:words[3]^=1;data.source.words=words.data();break;
        case 5:offsets[2]^=1;data.source.offsets=offsets.data();break;
        case 6:context[0]^=1;data.source.context=context.data();break;
        case 7:guards[0].offset=88;data.guards=guards.data();break;
        case 8:guards[0].allowed&=~1;data.guards=guards.data();break;
        case 9:progress[0].scalarWork=4u*1024*1024+1;data.progress=progress.data();break;
        case 10:badImage[0]=1;badMask[0]=0;data.image=badImage.data();data.mask=badMask.data();break;
        case 11:data.source.words=nullptr;break;
        }
        std::fill(loaded.begin(),loaded.end(),0x5a5a);std::fill(loadedMask.begin(),loadedMask.end(),0xa5a5);
        check(!cache.installPrepared(recipe,data,loaded.data(),loadedMask.data()) && cache.error && !cache.ready,"invalid prepared data accepted");
        check(std::all_of(loaded.begin(),loaded.end(),[](uint16_t v){return v==0x5a5a;}) && std::all_of(loadedMask.begin(),loadedMask.end(),[](uint16_t v){return v==0xa5a5;}),"invalid prepared data changed destination");
    }
    std::puts("PASS: prepared bitmap/mask, guards, positions/work and white-prefix proof match renderer; 12 malformed descriptors fail before writes");
}
#endif
int main(int argc,char **argv)try{
#ifdef POKERI_CARD_PREPARED
    preparedCheck();
#endif

    if(argc==2 && std::string(argv[1])=="--raster-batch"){batchCheck<CachedBatchReference>();return 0;}
    if(argc==2 && std::string(argv[1])=="--raster-batch-native"){batchCheck<pokeri::CachedBatch>();partialBatchCheck();return 0;}
    guardAddressCheck();
    if(argc==2 && std::string(argv[1])=="--raster-absolute"){absolute=controls=grants=true;argc=1;}
    if(argc==2 && std::string(argv[1])=="--raster-controls"){controls=true;grants=true;argc=1;}
    if(argc==2 && std::string(argv[1])=="--raster-grant"){grants=true;argc=1;}
#ifdef POKERI_LEDGER_FAST_CACHE
    {
        static unsigned endpoints[3]={};
        Fixture f(0,16,126);
        f.cache.timing=[](unsigned kind,unsigned){check(kind<3,"unknown timing boundary");++endpoints[kind];};
        unsigned before=grantHits;f.run();f.finish();
        check(endpoints[0]==1 && endpoints[1]==1 && !endpoints[2],"aggregate timing must see exactly one complete cached card");
        if(grants)check(grantHits>before,"aggregate timing must preserve completion grants");
    }
#endif
    for(bool reads:{false,true})for(bool rows:{false,true})for(unsigned align=0;align<16;++align)for(unsigned bg=0;bg<18;++bg){
        Fixture f(bg,align,126,rows);f.surface.planeReads=reads;
        for(unsigned c=0;c<6;++c)f.command(c);
        check(f.surface.planeAttempts>0,"guard did not attempt planar read");
        if(!reads)check(f.surface.planeAttempts==1,"unsupported plane reads should fall back once");
        for(unsigned c=6;c<79 && !f.reference.error;++c)f.command(c);f.finish();
        if(bg<16)check(f.cache.hits==unsigned(bg!=1 && bg!=15),"solid background guard admission differs");
        if(bg==17)check(f.cache.hits==1,"guarded random background declined");
    }
    for(int x:{-296,0,239,240,241,32750})for(int y:{-1200,-1000,0,126}){
        Fixture f(17,x,y);f.run();f.finish();
    }
    // Common face-up prefix: every alignment/background, subsequent copies
    // and explicit observations. A solid-white bitmap is not assumed: prepare
    // proves its exact colour and coverage against the complete recipe.
    for(bool rows:{false,true})for(unsigned align=0;align<16;++align)for(unsigned bg=0;bg<18;++bg){
        Fixture f(bg,align,126,rows);
        for(unsigned c=0;c<CardBackCache::WhiteCommands;++c)f.command(c);
        f.actual.observePixels();
        if(bg<16)check(f.cache.whiteHits==unsigned(bg!=1 && bg!=15),"white background guard admission differs");
        if(bg==17)check(f.cache.whiteHits==1,"white guarded random background declined");
        // Draw a synthetic rank copy, then an inset copy into the cached card.
        for(uint16_t w:{0x8000,5,170,0xe000,300,100,16,16,0x8000,23,148,0xe000,350,100,39,53})f.word(w);
        f.finish(true);
    }
    // Every word includes every opcode, fixed parameter and polyline vertex.
    for(unsigned mutate=0;mutate<CardBackCache::Words;++mutate){
        Fixture f;f.stream[mutate]^=1;
        for(uint16_t w:f.stream){if(f.reference.error)break;f.word(w);}
        f.finish(true);check(!f.cache.hits,"mutated recipe admitted");
    }
    // Every timer cut: merely inspecting recognition must keep the whole card
    // eligible for its final blit, including the non-raster opening commands.
    for(unsigned boundary=0;boundary<=CardBackCache::Commands;++boundary){
        Fixture f;
        for(unsigned c=0;c<boundary;++c)f.command(c);
        check(f.cache.sequenceIncoming()==(boundary>0 && boundary<CardBackCache::Commands),"partial sequence state differs");
        if(boundary==1)check(!f.actual.cachedPixels,"opening command unexpectedly deferred pixels");
        for(unsigned c=boundary;c<CardBackCache::Commands;++c)f.command(c);
        check(!f.cache.sequenceIncoming() && f.cache.hits==1 && !f.cache.prefixReplays,"timer inspection broke card recognition");
        f.finish(true);
    }
    // A rejected bitmap must still track an exact sequence for presentation.
    // Existing white card pixels are a common reason for guard refusal.
    for(unsigned bg:{1u,15u})for(unsigned boundary=6;boundary<=CardBackCache::Commands;++boundary){
        Fixture f(bg,3,126);
        for(unsigned c=0;c<boundary;++c)f.command(c);
        check(f.cache.guardMisses==1 && !f.cache.hits && !f.actual.cachedPixels,"unsafe bitmap was used");
        check(f.cache.sequenceIncoming()==(boundary<CardBackCache::Commands),"guard refusal lost presentation tracking");
        // Reads see the ordinary renderer's pixels even before completion.
        for(unsigned a=0;a<=f.reference.frameMask;++a)
            check(f.reference.readWord(a)==f.actual.readWord(a),"tracked fallback deferred pixels");
        for(unsigned c=boundary;c<CardBackCache::Commands;++c)f.command(c);
        check(!f.cache.sequenceIncoming() && !f.cache.hits && !f.cache.prefixReplays,"tracked fallback did not complete normally");
        f.finish(true);
    }
    // Mismatch and explicit abort release tracking without replaying pixels.
    for(unsigned boundary=6;boundary<CardBackCache::Commands;++boundary){
        Fixture f(1,3,126);
        for(unsigned c=0;c<boundary;++c)f.command(c);
        f.word(0x0802);f.word(0x9999);
        check(!f.cache.sequenceIncoming() && !f.cache.prefixReplays,"fallback mismatch retained tracking");
        f.finish(true);
        Fixture aborted(15,3,126);
        for(unsigned c=0;c<boundary;++c)aborted.command(c);
        aborted.actual.flushCard();
        check(!aborted.cache.sequenceIncoming() && !aborted.cache.prefixReplays,"fallback abort retained tracking");
        aborted.finish(true);
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
    if(argc>1){
        std::ifstream input(argv[1]);check(bool(input),"missing face-up catalog");
        std::string line;unsigned count=0;
        while(std::getline(input,line)){
            std::istringstream in(line);std::string tag;unsigned suit,rank;
            in>>tag>>suit>>rank;check(tag=="CARD" && suit>=1 && suit<=4 && rank<=14,"bad face-up catalog");
            std::vector<uint16_t> stream;unsigned value;while(in>>std::hex>>value)stream.push_back(value);
            for(bool rows:{false,true}){
                Fixture f(17,3,126,rows,true);
                for(unsigned i=0;i<stream.size();){
                    unsigned group=stream[i]>>10,n=group==2 || group==42?2:group==50?1:group==56?5:3;
                    check(group==2 || group==42 || group==50 || group==56 || group==32 || group==33 || group==49,"unknown catalog command");
                    check(i+n<=stream.size(),"truncated catalog command");
                    for(unsigned j=0;j<n;++j){uint16_t w=stream[i+j];if(group==32 && j)w+=j==1?3:126;f.word(w);}
                    i+=n;
                }
                f.finish(true);check(f.cache.whiteHits==1,"complete original face-up prefix not cached");
            }
            ++count;
        }
        check(count==60,"incomplete face-up selectors");
    }
    if(grants){
        Fixture first(17,3,126);first.run();first.finish();
        Fixture second(17,7,170);second.grant=first.grant;
        second.run();second.finish(true);
    }
    if(grants){
        grants=false;
        Fixture observed;
        CardBackCache::RasterGrant grant;
        for(uint16_t word:observed.stream){
            check(!observed.cache.rasterGrant(observed.actual,grant),"observer received raster grant");
            observed.word(word);
        }
        observed.finish(true);grants=true;
    }
    std::printf("PASS: card cache %u differential cases / %u back hits / %u white hits; pixels, prefix state/work, logs, mutations, observations and snapshots\n",cases,hits,whiteHits);
    if(grants){check(grantHits>0,"grant kernel never accepted");
        std::printf("PASS: %u model-granted raster completions\n",grantHits);}
    return 0;
}catch(const std::exception &e){std::fprintf(stderr,"FAIL case %u: %s\n",cases,e.what());return 1;}
