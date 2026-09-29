// Generate local artwork with the authoritative renderer proof passes. No ROM
// command, pixel or mask data belongs in this source or in version control.
#include "../src/board/CardBackCache.h"
#include "../amiga/generated/CardBackRecipe.h"
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>
using pokeri::CardBackCache;
namespace card_recipe=pokeri::card_recipe;
template<class T> static void array(std::ostream &out,const char *type,const char *name,const T *p,unsigned n){
    out<<"static const "<<type<<' '<<name<<"[] = {\n";
    for(unsigned i=0;i<n;++i){out<<"0x"<<std::hex<<uint32_t(p[i])<<std::dec<<',';if(i%12==11)out<<'\n';}
    out<<"\n};\n";
}
int main(int argc,char **argv){
    if(argc!=2){std::fprintf(stderr,"usage: card-back-prepare OUTPUT\n");return 1;}
    std::vector<uint16_t> image(CardBackCache::BitmapWords),mask(image.size());
    CardBackCache c;
    if(!c.prepare({card_recipe::words,card_recipe::offsets,card_recipe::context},image.data(),mask.data())){
        std::fprintf(stderr,"card preparation failed: %s\n",c.error?c.error:"allocation");return 1;}
    const std::string path=argv[1],temporary=path+".tmp";
    std::ofstream out(temporary);if(!out)return 1;
    out<<"// GENERATED from local ROMs and the shared renderer; DO NOT COMMIT.\n#pragma once\nnamespace card_prepared {\n";
    array(out,"uint16_t","sourceWords",card_recipe::words,CardBackCache::Words);
    array(out,"uint16_t","sourceOffsets",card_recipe::offsets,CardBackCache::Commands+1);
    array(out,"uint32_t","sourceContext",card_recipe::context,308);
    array(out,"uint16_t","image",image.data(),image.size());
    array(out,"uint16_t","mask",mask.data(),mask.size());
    out<<"static const pokeri::CardBackCache::Guard guards[] = {\n";
    for(unsigned i=0;i<c.guardCount;++i)out<<'{'<<c.guards[i].offset<<','<<c.guards[i].allowed<<','<<c.guards[i].rightWhite<<"},\n";
    out<<"};\nstatic const pokeri::CardBackCache::Progress progress[] = {\n";
    for(auto p:c.progress)out<<'{'<<p.x<<','<<p.y<<','<<p.scalarWork<<','<<p.rectangleWork<<"},\n";
    out<<"};\nstatic const pokeri::CardBackCache::Progress whiteProgress[] = {\n";
    for(auto p:c.whiteProgress)out<<'{'<<p.x<<','<<p.y<<','<<p.scalarWork<<','<<p.rectangleWork<<"},\n";
    out<<"};\nstatic const pokeri::CardBackCache::Progress rightWhiteProgress[] = {\n";
    for(auto p:c.rightWhiteProgress)out<<'{'<<p.x<<','<<p.y<<','<<p.scalarWork<<','<<p.rectangleWork<<"},\n";
    out<<"};\nstatic const pokeri::CardBackCache::Prepared data = {3,{sourceWords,sourceOffsets,sourceContext},image,mask,guards,progress,whiteProgress,rightWhiteProgress,"
       <<c.guardCount<<','<<c.coverage<<','<<(c.whiteReady?"true":"false")<<"};\n}\n";
    out.close();if(!out || std::rename(temporary.c_str(),path.c_str()))return 1;
    std::printf("Prepared card cache: %u pixels, %u guards, %u progress entries; white=%u\n",c.coverage,c.guardCount,unsigned(CardBackCache::Commands),c.whiteReady);
}
