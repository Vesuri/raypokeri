// Extract sound-call arguments from FS-UAE pre-instruction register records.
// No ROM bytes are read or emitted. Addresses are supplied by the ELF checker.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
struct Reader {
    std::vector<uint8_t> bytes; size_t pos=0;
    void skip(size_t n) { if(n>bytes.size()-pos){fprintf(stderr,"truncated trace\n");exit(1);} pos+=n; }
    uint32_t word(){size_t p=pos;skip(4);uint32_t v;memcpy(&v,bytes.data()+p,4);return v;}
};
int main(int argc,char **argv){
    if(argc!=6){fprintf(stderr,"usage: sound_trace_reduce ROM_BASE ENTRY RETURN BACKEND TRACE\n");return 1;}
    uint32_t base=strtoul(argv[1],nullptr,16),entry=strtoul(argv[2],nullptr,16),ret=strtoul(argv[3],nullptr,16),backend=strtoul(argv[4],nullptr,16);
    Reader r;FILE *f=fopen(argv[5],"rb");if(!f)return 1;
    fseek(f,0,SEEK_END);long length=ftell(f);if(length<0)return 1;rewind(f);r.bytes.resize(length);
    if(fread(r.bytes.data(),1,length,f)!=size_t(length))return 1;fclose(f);
    uint32_t fields=r.word(),sections=r.word();if(!sections)return 1;
    uint32_t offset=base-r.word();r.skip(size_t(sections-1)*4);r.skip(16);
    for(int i=0;i<3;++i){uint32_t n=r.word();r.skip(n);}r.word();r.word();
    for(uint32_t field=0;field<fields;++field){
        uint32_t n=r.word();if(n!=520)return 1;r.skip(n);
        n=r.word();if(n!=0 && n!=1024)return 1;r.skip(n);
        for(int i=0;i<2;++i){uint32_t size=r.word(),count=r.word();r.skip(size_t(size)*count);}
        r.word();r.word();uint32_t count=r.word();size_t end=r.pos+size_t(count)*4;
        while(r.pos<end){
            uint32_t pc=0xffffffff,v;
            do {v=r.word();if(v<0xffff0000 && pc==0xffffffff)pc=v;}while(v<0xffff0000);
            uint32_t regs[17];for(auto &reg:regs)reg=r.word();
            if(pc==offset+entry)printf("G %u %u %u\n",regs[0]&255,regs[1]&255,regs[2]&255);
            if(pc==backend)printf("P %u %u\n",regs[1],regs[2]&255);
            if(pc==offset+ret)puts("R");
        }
        if(r.pos!=end)return 1;n=r.word();r.word();r.skip(n);
    }
    if(r.pos!=r.bytes.size()){fprintf(stderr,"unparsed trailing bytes\n");return 1;}
}
