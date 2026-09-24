#ifndef POKERI_ROM_IDENTITY_H
#define POKERI_ROM_IDENTITY_H
// Host-only macOS verification. The original files are checked before mutation.
#include <CommonCrypto/CommonDigest.h>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
inline void verifyRomIdentity(const uint8_t *image) {
    static const char *hashes[]={
        "2841c2393d469c744eb5e575b08f1e4f13320e73fd205eb27d2ea2cf4b59decd",
        "fd87d156b71753d7ba03f548bf12bc3fee1d16477d1e89a9e9fe36521808ec8e",
        "3facfb79dfd6942a197bc6f9456712cb1a0de92e0035a589711988148f07c0a7",
        "ae1b91f898d8d69a36fde41bff1139c94c8b9cb93e77b4c697ddc43a362d244b"};
    for(unsigned chip=0;chip<4;++chip){
        unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256(image+chip*65536,65536,digest);
        char hex[65];for(unsigned i=0;i<32;++i)std::snprintf(hex+2*i,3,"%02x",digest[i]);
        if(std::string(hex)!=hashes[chip])throw std::runtime_error("ROM SHA-256 mismatch before relocation/checksum bypass");
    }
}
#endif
