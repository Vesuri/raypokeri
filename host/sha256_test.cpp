#include "../src/native/Sha256.h"
#include <CommonCrypto/CommonDigest.h>
#include <vector>
#include <cassert>
#include <cstdio>
int main(){for(unsigned n: {0u,1u,3u,55u,56u,63u,64u,65u,127u,65536u}){std::vector<uint8_t> data(n);for(unsigned i=0;i<n;++i)data[i]=(i*53+i/7)&255;uint8_t a[32],b[32];pokeri::sha256(data.data(),n,a);CC_SHA256(data.data(),n,b);for(unsigned i=0;i<32;++i)assert(a[i]==b[i]);}puts("PASS portable SHA-256 vs system SHA-256: empty, padding/block boundaries and chip size");}
