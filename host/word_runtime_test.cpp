#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
extern "C" uint64_t __muldi3(uint64_t,uint64_t);
extern "C" uint64_t __udivdi3(uint64_t,uint64_t);
extern "C" void pokeriRuntimeFault(const char *s){throw std::runtime_error(s);}
int main(){
    uint64_t seed=0x732abcdef0123456ULL;
    auto next=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
    auto check=[](uint64_t a,uint64_t b){
        if(__muldi3(a,b)!=a*b || (b && __udivdi3(a,b)!=a/b))std::abort();
    };
    for(unsigned i=0;i<64;++i)for(unsigned j=0;j<64;++j){
        uint64_t a=uint64_t(1)<<i,b=uint64_t(1)<<j;
        check(a,b);check(a-1,b);check(a,b-1);check(~a,b);check(a,~b);
    }
    for(unsigned i=0;i<10000;++i){
        uint64_t a=uint32_t(next()),b=uint16_t(next());
        check(a,b);check(b,a);check(uint16_t(a),b);
    }
    for(unsigned i=0;i<50000;++i){uint64_t a=next(),b=next();check(a,b);}
    bool fault=false;try{__udivdi3(1,0);}catch(const std::runtime_error&){fault=true;}
    if(!fault)std::abort();
    puts("PASS: integer wide multiply/divide boundary and randomized cases; zero divisor stops");
}
