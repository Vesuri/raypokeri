// Offline T12 study: replay captured bus traffic through the ordinary device.
// No generated artwork or original command words are embedded in this tool.
#include "../src/board/Hd63484.h"
#include "../src/board/PlanarSurface.h"
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
using pokeri::Hd63484;
static Hd63484 video;
static std::vector<uint16_t> previous(0x40000);
static pokeri::PlanarSurface planar;
static std::vector<uint16_t> planes(pokeri::PlanarLayout::storageWords(0x40000,true)), previousPlanes(planes.size());
static unsigned long long planarWords,planarRuns;
static unsigned long long commands,words,changedWords,runs,changedCommands;
static unsigned long long groupCommands[64]={},groupWords[64]={},groupRuns[64]={};
static void check(bool good,const char *message){if(!good)throw std::runtime_error(message);}
static void completed(const uint16_t *w,unsigned n,bool executed){
    check(executed && !video.error,"unsupported captured command");
    ++commands;words+=n;unsigned g=w[0]>>10; ++groupCommands[g];
    bool inRun=false,changed=false;
    for(unsigned i=0;i<previous.size();++i){
        bool different=previous[i]!=video.frame[i];
        if(different){
            ++changedWords;++groupWords[g];changed=true;
            if(!inRun){++runs;++groupRuns[g];}
            previous[i]=video.frame[i];
            planar.writeWord(i,video.frame[i]);
        }
        inRun=different;
    }
    if(changed)++changedCommands;
    inRun=false;
    for(unsigned i=0;i<planes.size();++i){
        bool different=planes[i]!=previousPlanes[i];
        if(different){++planarWords;if(!inRun)++planarRuns;previousPlanes[i]=planes[i];}
        inRun=different;
    }
}
int main(int argc,char **argv){try{
    if(argc!=3){fprintf(stderr,"usage: boot-artwork-study TRACE.csv EXPECTED-vram.bin\n");return 2;}
    std::ifstream trace(argv[1]);check(bool(trace),"cannot open trace");
    std::string line;check(bool(std::getline(trace,line)),"missing CSV header");
    check(line=="instruction,pc,address,size,direction,value,device,cpu_address","unexpected CSV format");
    planar.attach(planes.data(),0x40000,true);
    video.commandLog=completed;
    unsigned long long accesses=0,reads=0;
    while(std::getline(trace,line)){
        unsigned long long instruction;unsigned pc,address,size,value,cpu;char dir,name[32];
        check(sscanf(line.c_str(),"%llu,%x,%x,%u,%c,%x,%31[^,],%x",&instruction,&pc,&address,&size,&dir,&value,name,&cpu)==8,"invalid CSV row");
        if(std::string(name)!="hd63484")continue;
        check(address>=0xf6000 && address+size<=0xf6004 && (size==1 || size==2 || size==4),"unsupported device access");
        check(dir=='R' || dir=='W',"invalid direction");++accesses;
        unsigned result=0;
        for(unsigned i=0;i<size;++i){
            unsigned offset=address+i-0xf6000;
            if(dir=='W')video.write8(offset,uint8_t(value>>((size-1-i)*8)));
            else result=(result<<8)|video.read8(offset);
            check(!video.error,"device error replaying bus trace");
        }
        if(dir=='R'){++reads;check(result==value,"captured read disagrees with replay");}
    }
    check(trace.eof(),"trace read failed");check(commands>0,"no captured commands");
    std::ifstream expected(argv[2],std::ios::binary);check(bool(expected),"cannot open expected VRAM");
    for(unsigned i=0;i<=video.frameMask;++i){
        int hi=expected.get(),lo=expected.get();
        check(hi>=0 && lo>=0,"truncated expected VRAM");
        check(video.readWord(i)==uint16_t((hi<<8)|lo),"final VRAM mismatch");
        check(planar.readWord(i)==video.readWord(i),"planar round-trip mismatch");
    }
    check(expected.get()==std::char_traits<char>::eof(),"excess expected VRAM bytes");
    printf("PASS: %llu bus accesses, %llu matching reads, %u VRAM bytes\n",accesses,reads,2*(video.frameMask+1));
    printf("commands=%llu recipe_words=%llu changed_commands=%llu delta_words=%llu delta_runs=%llu\n",commands,words,changedCommands,changedWords,runs);
    printf("Packed command-boundary delta estimate: %llu bytes (2/word + 6/run + 4/command offset), before semantic metadata\n",2*changedWords+6*runs+4*(commands+1));
    printf("Planar command-boundary deltas: %llu words, %llu runs, %llu bytes including command offsets, before semantic metadata\n",planarWords,planarRuns,2*planarWords+6*planarRuns+4*(commands+1));
    puts("group,command,commands,changed_words,runs");
    for(unsigned g=0;g<64;++g)if(groupCommands[g])printf("%u,%s,%llu,%llu,%llu\n",g,Hd63484::mnemonic(uint16_t(g<<10)),groupCommands[g],groupWords[g],groupRuns[g]);
    return 0;
}catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
