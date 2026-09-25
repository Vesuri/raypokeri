#ifndef POKERI_STARTUP_CACHE_H
#define POKERI_STARTUP_CACHE_H
// Local ROM-derived state stays in the repository's ignored tmp directory.
// Hash the executable too: device/core changes must invalidate saved state even
// when their serialization layout happens to remain compatible.
#include <CommonCrypto/CommonDigest.h>
#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <climits>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
inline std::string startupCachePath(const std::string &rom) {
    char root[PATH_MAX];
    if(!realpath((rom+"/..").c_str(),root))return {};
    std::string directory=std::string(root)+"/tmp";
    mkdir(directory.c_str(),0755);
    uint32_t size=0;_NSGetExecutablePath(nullptr,&size);
    std::vector<char> path(size);
    if(_NSGetExecutablePath(path.data(),&size))return {};
    FILE *f=fopen(path.data(),"rb");if(!f)return {};
    CC_SHA256_CTX context;CC_SHA256_Init(&context);
    unsigned char bytes[16384],digest[CC_SHA256_DIGEST_LENGTH];size_t n;
    while((n=fread(bytes,1,sizeof bytes,f)))CC_SHA256_Update(&context,bytes,CC_LONG(n));
    bool failed=ferror(f);fclose(f);if(failed)return {};
    CC_SHA256_Final(digest,&context);
    char hex[65];for(unsigned i=0;i<32;++i)std::snprintf(hex+2*i,3,"%02x",digest[i]);
    return directory+"/sdl-clean-start-"+hex+".state";
}
#endif
