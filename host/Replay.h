#ifndef POKERI_REPLAY_WRITER_H
#define POKERI_REPLAY_WRITER_H
#include <cstdio>
#include <cstdint>
#include <stdexcept>
#include <string>
// Diagnostic timing/input records, never CPU or device results to inject.
// All files are local captures in tmp/, not redistributable fixtures.
struct ReplayWriter {
    FILE *file=nullptr;
    uint64_t previousInstruction=0,previousCycle=0,lastBusInstruction=UINT64_MAX;
    uint32_t previousPc=0;
    ~ReplayWriter(){if(file)fclose(file);}
    void integer(uint32_t value){do{unsigned byte=value&127;value>>=7;if(value)byte|=128;if(fputc(byte,file)==EOF)throw std::runtime_error("replay write failed");}while(value);}
    void open(const std::string &path){file=fopen(path.c_str(),"wb");if(!file)throw std::runtime_error("cannot create replay");if(fwrite("PKREPLAY\2",1,9,file)!=9)throw std::runtime_error("replay header write failed");}
    void event(unsigned kind,uint64_t instruction,uint64_t cycle,uint32_t pc,uint32_t a=0,uint32_t b=0){
        if(!file)return;
        if(instruction<previousInstruction || cycle<previousCycle || instruction>UINT32_MAX || cycle>UINT32_MAX)throw std::runtime_error("replay counter range/order");
        integer(kind);integer(uint32_t(instruction-previousInstruction));integer(uint32_t(cycle-previousCycle));int32_t delta=int32_t(pc)-int32_t(previousPc);integer((uint32_t(delta)<<1)^uint32_t(delta>>31));
        if(kind==2 || kind==3 || kind==5 || kind==7)integer(a);
        if(kind==2 || kind==3 || kind==5)integer(b);
        previousPc=pc;
        previousInstruction=instruction;previousCycle=cycle;
    }
    void bus(uint64_t instruction,uint64_t cycle,uint32_t pc){if(file && lastBusInstruction!=instruction){event(1,instruction,cycle,pc);lastBusInstruction=instruction;}}
    void close(){if(file){FILE*f=file;file=nullptr;if(fclose(f))throw std::runtime_error("replay close failed");}}
};
#endif
