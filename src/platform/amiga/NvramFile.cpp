#include "NvramFile.h"
#include "RetainedAccounting.h"
#include <proto/dos.h>
#include <dos/dos.h>
extern "C" uint16_t pokeriWhdLoad;
// With little memory left after the slave's reservation, WHDLoad's write cache
// hangs at exit after creating a new file (docs/release.md), so the installer
// creates both slots. Check them before board/display allocation.
const char *checkWhdLoadSaveSlots(){
    if(!pokeriWhdLoad)return nullptr;
    BPTR f=Open("nvram.bin",MODE_OLDFILE);
    if(!f)return "missing save slot; run installer with Keep";
    LONG end=Seek(f,0,OFFSET_END);
    bool okay=end>=0 && Seek(f,0,OFFSET_CURRENT)==32768;
    Close(f);
    if(!okay)return "invalid save slot; existing files preserved";
    f=Open("accounting.bin",MODE_OLDFILE);
    if(!f)return "missing save slot; run installer with Keep";
    pokeri::RetainedAccounting image;uint8_t extra;
    LONG n=Read(f,image.bytes.data(),image.bytes.size());LONG tail=Read(f,&extra,1);
    Close(f);
    okay=n==LONG(image.bytes.size()) && tail==0 && (image.isFresh() || image.valid());
    return okay?nullptr:"invalid save slot; existing files preserved";
}
static const char *save(const char *name,const void *data,unsigned size){
    BPTR f=Open(name,MODE_NEWFILE);if(!f)return "cannot create save";
    LONG written=Write(f,(void*)data,size);LONG closed=Close(f);
    return written==LONG(size) && closed?nullptr:"save write failed";
}
const char *loadNvram(pokeri::Nvram &nvram){
    BPTR f=Open("nvram.bin",MODE_OLDFILE);
    if(!f)return IoErr()==ERROR_OBJECT_NOT_FOUND?nullptr:"cannot open NVRAM";
    LONG n=Read(f,nvram.bytes.data(),nvram.bytes.size());uint8_t extra;LONG tail=Read(f,&extra,1);Close(f);
    return n==LONG(nvram.bytes.size()) && tail==0?nullptr:"invalid NVRAM size";
}
const char *saveNvram(const pokeri::Nvram &nvram){
    return save("nvram.bin",nvram.bytes.data(),nvram.bytes.size());
}

const char *loadAccounting(uint8_t *memory,bool &loaded){
    loaded=false;BPTR f=Open("accounting.bin",MODE_OLDFILE);
    if(!f)return IoErr()==ERROR_OBJECT_NOT_FOUND?nullptr:"cannot open accounting";
    pokeri::RetainedAccounting image;
    LONG n=Read(f,image.bytes.data(),image.bytes.size());uint8_t extra;LONG tail=Read(f,&extra,1);Close(f);
    if(n!=LONG(image.bytes.size()) || tail!=0)return "invalid retained accounting";
    if(image.isFresh())return nullptr; // installer slot: original cold initialization
    if(!image.decode(memory))return "invalid retained accounting";
    loaded=true;return nullptr;
}
const char *saveAccounting(const uint8_t *memory){
    pokeri::RetainedAccounting image;image.encode(memory);
    return save("accounting.bin",image.bytes.data(),image.bytes.size());
}
