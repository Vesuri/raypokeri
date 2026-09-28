#include "NvramFile.h"
#include "RetainedAccounting.h"
#include <proto/dos.h>
#include <dos/dos.h>
static bool exists(const char *path){BPTR f=Open(path,MODE_OLDFILE);if(!f)return false;Close(f);return true;}
const char *loadNvram(pokeri::Nvram &nvram){
    BPTR f=Open("nvram.bin",MODE_OLDFILE);
    if(!f)return IoErr()==ERROR_OBJECT_NOT_FOUND?nullptr:"cannot open NVRAM";
    LONG n=Read(f,nvram.bytes.data(),nvram.bytes.size());uint8_t extra;LONG tail=Read(f,&extra,1);Close(f);
    return n==LONG(nvram.bytes.size()) && tail==0?nullptr:"invalid NVRAM size";
}
const char *saveNvram(const pokeri::Nvram &nvram){
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
    if(n!=LONG(image.bytes.size()) || tail!=0 || !image.decode(memory))return "invalid retained accounting";
    loaded=true;return nullptr;
}
const char *saveAccounting(const uint8_t *memory){
    pokeri::RetainedAccounting image;image.encode(memory);
    BPTR f=Open("accounting.new",MODE_NEWFILE);if(!f)return "cannot create accounting temporary file";
    LONG n=Write(f,image.bytes.data(),image.bytes.size());LONG closed=Close(f);
    if(n!=LONG(image.bytes.size()) || !closed)return "accounting write failed";
    bool old=exists("accounting.bin");
    if(old){if(exists("accounting.bak") && !DeleteFile("accounting.bak"))return "cannot replace accounting backup";
        if(!Rename("accounting.bin","accounting.bak"))return "cannot back up accounting";}
    if(!Rename("accounting.new","accounting.bin")){if(old)Rename("accounting.bak","accounting.bin");return "cannot install accounting";}
    return nullptr;
}
