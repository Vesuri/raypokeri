#ifndef POKERI_PREPARED_HOOK_H
#define POKERI_PREPARED_HOOK_H
#include "Hook.h"
// Platform buses are concrete types so checked data access can be inlined.
// The generic HookBus implementation remains the independent fallback.
namespace pokeri {
namespace prepared_detail {
inline uint32_t mask(unsigned n){return n==1?0xff:n==2?0xffff:0xffffffffu;}
struct Resolved {Ea kind;unsigned reg;uint32_t address,value;};
inline bool resolve(const Operand &o,unsigned size,Registers &r,uint32_t ext,Resolved &v){
    v.kind=o.kind;v.reg=unsigned(o.reg);v.address=0;v.value=0;
    if(o.kind==Ea::none)return true;
    if(o.kind==Ea::data || o.kind==Ea::address){
        if(v.reg>7)return false;
        v.value=o.kind==Ea::data?r.d[v.reg]:r.a[v.reg];return true;
    }
    if(o.kind==Ea::immediate){v.value=ext&mask(size);return true;}
    if(o.kind==Ea::absolute_long){v.address=ext;return true;}
    if(o.kind==Ea::absolute_word){v.address=uint32_t(int32_t(int16_t(ext)));return true;}
    bool relative=o.kind==Ea::pc_displacement || o.kind==Ea::pc_indexed;
    if(!relative && v.reg>7)return false;
    v.address=relative?r.pc+unsigned(o.extension):r.a[v.reg];
    unsigned step=size==1 && v.reg==7?2:size;
    switch(o.kind){
    case Ea::indirect:break;
    case Ea::postincrement:r.a[v.reg]+=step;break;
    case Ea::predecrement:r.a[v.reg]-=step;v.address=r.a[v.reg];break;
    case Ea::displacement:case Ea::pc_displacement:v.address+=int32_t(int16_t(ext));break;
    case Ea::indexed:case Ea::pc_indexed:{
        if(ext&0x0700)return false; // 68000 brief extension, no scaled/full EA
        unsigned index=(ext>>12)&7;
        uint32_t n=ext&0x8000?r.a[index]:r.d[index];
        if(!(ext&0x0800))n=uint32_t(int32_t(int16_t(n)));
        v.address+=n+int32_t(int8_t(ext));break;
    }
    default:return false;
    }
    return true;
}
template<class Bus> bool read(const Resolved &v,unsigned size,Bus &bus,uint32_t &n){
    if(v.kind==Ea::none){n=0;return true;}
    if(v.kind==Ea::data || v.kind==Ea::address || v.kind==Ea::immediate){n=v.value&mask(size);return true;}
    return bus.read(v.address,size,n);
}
template<class Bus> bool write(const Resolved &v,unsigned size,Registers &r,Bus &bus,uint32_t n){
    n&=mask(size);
    if(v.kind==Ea::data){r.d[v.reg]=(r.d[v.reg]&~mask(size))|n;return true;}
    if(v.kind==Ea::address){if(size==1)return false;r.a[v.reg]=size==2?uint32_t(int32_t(int16_t(n))):n;return true;}
    if(v.kind==Ea::none || v.kind==Ea::immediate || v.kind==Ea::pc_displacement || v.kind==Ea::pc_indexed)return false;
    return bus.write(v.address,size,n);
}
inline void nz(Registers &r,uint32_t n,unsigned size){
    r.sr=uint16_t((r.sr&~15u)|(!(n&mask(size))?4:0)|((n>>(size*8-1))&1?8:0));
}
}
template<class Bus> bool executePreparedHook(const PreparedHook &prepared,Registers &r,Bus &bus){
    using namespace prepared_detail;
    const Hook &h=prepared.hook;
    if((h.size!=1 && h.size!=2 && h.size!=4) || h.length<2 || h.length>10 || (h.length&1))return false;
    // The audited BTST sites all address bytes in memory. Do not claim
    // support for the different long-register/static-immediate form.
    if((h.operation==Operation::bit_test || h.operation==Operation::bit_test_register) && (h.size!=1 || h.dest.kind==Ea::data))return false;
    // Common bus instruction forms avoid constructing generic EA records.
    // Check both shapes before reading anything: a fallback must not repeat
    // a peripheral read or a postincrement side effect.
    auto common=[](const Operand &o){return o.kind==Ea::data || o.kind==Ea::indirect || o.kind==Ea::postincrement || o.kind==Ea::displacement;};
    if(h.operation==Operation::move && h.size<=2 &&
       (common(h.source) || h.source.kind==Ea::immediate) && common(h.dest)){
        uint32_t value=0,ext=0,address=0;
        const Operand &s=h.source,&d=h.dest;
        if((s.kind!=Ea::immediate && (s.reg<0 || s.reg>7)) || d.reg<0 || d.reg>7)return false;
        if(s.kind==Ea::data)value=r.d[unsigned(s.reg)]&mask(h.size);
        else if(s.kind==Ea::immediate){value=prepared.sourceExtension&mask(h.size);}
        else {
            address=r.a[unsigned(s.reg)];
            if(s.kind==Ea::displacement){ext=prepared.sourceExtension;address+=int32_t(int16_t(ext));}
            if(s.kind==Ea::postincrement)r.a[unsigned(s.reg)]+=h.size==1 && s.reg==7?2:h.size;
            if(!bus.read(address,h.size,value))return false;
        }
        if(d.kind==Ea::data)r.d[unsigned(d.reg)]=(r.d[unsigned(d.reg)]&~mask(h.size))|(value&mask(h.size));
        else {
            address=r.a[unsigned(d.reg)];
            if(d.kind==Ea::displacement){ext=prepared.destExtension;address+=int32_t(int16_t(ext));}
            if(d.kind==Ea::postincrement)r.a[unsigned(d.reg)]+=h.size==1 && d.reg==7?2:h.size;
            if(!bus.write(address,h.size,value&mask(h.size)))return false;
        }
        nz(r,value,h.size);r.pc+=h.length;return true;
    }
    if(h.operation==Operation::bit_test && h.source.kind==Ea::immediate &&
       (h.dest.kind==Ea::displacement || h.dest.kind==Ea::indirect) && h.dest.reg>=0 && h.dest.reg<8 && h.source.extension>=2){
        uint32_t bit,displacement=0,value;
        bit=prepared.sourceExtension;
        if(h.dest.kind==Ea::displacement)displacement=prepared.destExtension;
        if(!bus.read(r.a[unsigned(h.dest.reg)]+int32_t(int16_t(displacement)),1,value))return false;
        r.sr=uint16_t((r.sr&~4u)|((value&(1u<<(bit&7)))?0:4));r.pc+=h.length;return true;
    }
    Resolved source,dest;uint32_t a=0,b=0,n=0;
    // Source read precedes destination EA evaluation: MOVE (An)+,(An)+ depends on it.
    if(!resolve(h.source,h.size,r,prepared.sourceExtension,source) || !read(source,h.size,bus,a) || !resolve(h.dest,h.size,r,prepared.destExtension,dest))return false;
    if(h.operation!=Operation::move && h.operation!=Operation::clear && !read(dest,h.size,bus,b))return false;
    switch(h.operation){
    case Operation::move:if(!write(dest,h.size,r,bus,a))return false;if(dest.kind!=Ea::address)nz(r,a,h.size);break;
    // Match the pinned host reference: CLR emits its write, without a dummy read.
    case Operation::clear:if(!write(dest,h.size,r,bus,0))return false;nz(r,0,h.size);break;
    case Operation::test:nz(r,b,h.size);break;
    case Operation::compare:{
        n=(b-a)&mask(h.size);nz(r,n,h.size);
        uint32_t sign=uint32_t(1)<<(h.size*8-1);
        if(((b^a)&(b^n)&sign))r.sr|=2;
        if((a&mask(h.size))>(b&mask(h.size)))r.sr|=1;
        break;
    }
    case Operation::bit_test:case Operation::bit_test_register:
        r.sr=uint16_t((r.sr&~4u)|((b&(uint32_t(1)<<(a&(dest.kind==Ea::data?31:7))))?0:4));break;
    case Operation::or_bits:case Operation::and_bits:
        n=h.operation==Operation::or_bits?(b|a):(b&a);
        if(!write(dest,h.size,r,bus,n))return false;
        nz(r,n,h.size);break;
    default:return false;
    }
    r.pc+=h.length;return true;
}
}

#endif
