#include "NvramFile.h"
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
