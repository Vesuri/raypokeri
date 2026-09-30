#include "NvramFile.h"
#include "RetainedAccounting.h"
#include <proto/dos.h>
#include <dos/dos.h>
extern "C" uint16_t pokeriWhdLoad;
// kickfs has no ACTION_RENAME_OBJECT. Keep the old complete image in a backup
// before replacement, using only its supported Open/Read/Write/Close operations.
static const char *saveKickfs(const char *name,const char *backup,const void *data,unsigned size){
    BPTR old=Open(name,MODE_OLDFILE);
    if(old){
        uint8_t *previous=new uint8_t[size];
        if(!previous){Close(old);return "cannot allocate save backup";}
        LONG got=Read(old,previous,size);uint8_t extra;LONG tail=Read(old,&extra,1);Close(old);
        if(got!=LONG(size) || tail!=0){delete[] previous;return "invalid previous save; refusing overwrite";}
        BPTR out=Open(backup,MODE_NEWFILE);
        if(!out){delete[] previous;return "cannot create save backup";}
        LONG written=Write(out,previous,size);LONG closed=Close(out);delete[] previous;
        if(written!=LONG(size) || !closed)return "save backup write failed";
    }else if(IoErr()!=ERROR_OBJECT_NOT_FOUND)return "cannot read previous save";
    BPTR out=Open(name,MODE_NEWFILE);if(!out)return "cannot create save";
    LONG written=Write(out,(void*)data,size);LONG closed=Close(out);
    return written==LONG(size) && closed?nullptr:"save write failed; previous image remains in .bak";
}
static bool exists(const char *path){BPTR f=Open(path,MODE_OLDFILE);if(!f)return false;Close(f);return true;}
const char *loadNvram(pokeri::Nvram &nvram){
    BPTR f=Open("nvram.bin",MODE_OLDFILE);
    if(!f)return IoErr()==ERROR_OBJECT_NOT_FOUND?nullptr:"cannot open NVRAM";
    LONG n=Read(f,nvram.bytes.data(),nvram.bytes.size());uint8_t extra;LONG tail=Read(f,&extra,1);Close(f);
    return n==LONG(nvram.bytes.size()) && tail==0?nullptr:"invalid NVRAM size";
}
const char *saveNvram(const pokeri::Nvram &nvram){
    if(pokeriWhdLoad)return saveKickfs("nvram.bin","nvram.bak",nvram.bytes.data(),nvram.bytes.size());
    BPTR f=Open("nvram.new",MODE_NEWFILE);if(!f)return "cannot create NVRAM temporary file";
    LONG n=Write(f,(void*)nvram.bytes.data(),nvram.bytes.size());LONG closed=Close(f);
    if(n!=LONG(nvram.bytes.size()) || !closed)return "NVRAM write failed";
    bool old=exists("nvram.bin");
    if(old){if(exists("nvram.bak") && !DeleteFile("nvram.bak"))return "cannot replace NVRAM backup";
        if(!Rename("nvram.bin","nvram.bak"))return "cannot back up NVRAM";}
    if(!Rename("nvram.new","nvram.bin")){if(old)Rename("nvram.bak","nvram.bin");return "cannot install NVRAM";}
    return nullptr;
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
    if(pokeriWhdLoad)return saveKickfs("accounting.bin","accounting.bak",image.bytes.data(),image.bytes.size());
    BPTR f=Open("accounting.new",MODE_NEWFILE);if(!f)return "cannot create accounting temporary file";
    LONG n=Write(f,image.bytes.data(),image.bytes.size());LONG closed=Close(f);
    if(n!=LONG(image.bytes.size()) || !closed)return "accounting write failed";
    bool old=exists("accounting.bin");
    if(old){if(exists("accounting.bak") && !DeleteFile("accounting.bak"))return "cannot replace accounting backup";
        if(!Rename("accounting.bin","accounting.bak"))return "cannot back up accounting";}
    if(!Rename("accounting.new","accounting.bin")){if(old)Rename("accounting.bak","accounting.bin");return "cannot install accounting";}
    return nullptr;
}
