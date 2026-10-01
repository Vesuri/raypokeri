#include "NvramFile.h"
#include "RetainedAccounting.h"
#include <proto/dos.h>
#include <dos/dos.h>
extern "C" uint16_t pokeriWhdLoad;
// Slave-provided resload_SaveFile(D0=size, A0=name, A1=data) -> D0, or 0.
extern "C" void *pokeriWhdSave;
// Under WHDLoad every file operation can switch to the OS, so each file is
// read with one Read and written with one call. WHDLoad's write cache can hang
// at exit after a new file is created when free memory is low, so the
// installer creates both files and WHDLoad runs never create one.
enum ReadResult {Loaded,Missing,Invalid};
static ReadResult readExact(const char *name,uint8_t *data,unsigned size){
    BPTR f=Open(name,MODE_OLDFILE);
    if(!f)return IoErr()==ERROR_OBJECT_NOT_FOUND?Missing:Invalid;
    // One extra byte detects an oversized file within the same Read.
    uint8_t *buffer=new uint8_t[size+1];
    LONG n=buffer?Read(f,buffer,size+1):-1;Close(f);
    if(n==LONG(size))for(unsigned i=0;i<size;++i)data[i]=buffer[i];
    delete[] buffer;
    return n==LONG(size)?Loaded:Invalid;
}
static const char *missing(){return pokeriWhdLoad?"missing save slot; run installer with Keep":nullptr;}
static const char *save(const char *name,const void *data,unsigned size){
    if(pokeriWhdSave){
        register ULONG d0 asm("d0")=size;register const char *a0 asm("a0")=name;register const void *a1 asm("a1")=data;
        asm volatile("jsr (%3)":"+d"(d0),"+a"(a0),"+a"(a1):"a"(pokeriWhdSave):"d1","cc","memory");
        return d0?nullptr:"save write failed";
    }
    BPTR f=Open(name,MODE_NEWFILE);if(!f)return "cannot create save";
    LONG written=Write(f,(void*)data,size);LONG closed=Close(f);
    return written==LONG(size) && closed?nullptr:"save write failed";
}
const char *loadNvram(pokeri::Nvram &nvram){
    switch(readExact("nvram.bin",nvram.bytes.data(),nvram.bytes.size())){
    case Loaded:return nullptr;
    case Missing:return missing();
    default:return "invalid NVRAM; existing file preserved";
    }
}
const char *saveNvram(const pokeri::Nvram &nvram){
    return save("nvram.bin",nvram.bytes.data(),nvram.bytes.size());
}

const char *loadAccounting(uint8_t *memory,bool &loaded){
    loaded=false;pokeri::RetainedAccounting image;
    switch(readExact("accounting.bin",image.bytes.data(),image.bytes.size())){
    case Loaded:break;
    case Missing:return missing();
    default:return "invalid retained accounting; existing file preserved";
    }
    if(image.isFresh())return nullptr; // installer slot: original cold initialization
    if(!image.decode(memory))return "invalid retained accounting; existing file preserved";
    loaded=true;return nullptr;
}
const char *saveAccounting(const uint8_t *memory){
    pokeri::RetainedAccounting image;image.encode(memory);
    return save("accounting.bin",image.bytes.data(),image.bytes.size());
}
