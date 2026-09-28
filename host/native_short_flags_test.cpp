// Execute the actual assembled sentinel body and compare its stacked CCR with
// Musashi executing independent synthetic CMP.L/TST.L instructions. No ROMs.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
static std::array<unsigned char,1048576> memory;
static unsigned expectedException=0;
static unsigned read(unsigned a,unsigned n){assert(a+n<=memory.size());unsigned v=0;while(n--)v=(v<<8)|memory[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){assert(a+n<=memory.size());while(n){--n;memory[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){write(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
void pokeri_exception(unsigned vector){assert(vector==expectedException && expectedException!=0);expectedException=0;}
}
int main(int argc,char **argv){
    assert(argc==48);FILE *file=fopen(argv[1],"rb");assert(file);
    unsigned length=fread(memory.data()+0x1000,1,1024,file);assert(feof(file) && length && length<1024);fclose(file);
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    const unsigned values[]={0,1,0x217e,0x40b00,0x7fffffff,0x80000000,0xfffffffe,0xffffffff};
    unsigned checks=0;
    for(unsigned type=0;type<4;++type)for(unsigned flags=0;flags<32;++flags)for(unsigned lhs:values)for(unsigned rhs:values){
        // Newly assembled synthetic forms: CMP.L 4(A2),D0; TST.L (A2);
        // CMP.L 8(A0),D4; TST.L (A0). These are not ROM excerpts.
        const unsigned op[]={0xb0aa,0x4a92,0xb8a8,0x4a90};
        write(0x200,2,op[type]);write(0x202,2,type==2?8:4);
        m68k_set_reg(M68K_REG_SR,0x2000|flags);m68k_set_reg(M68K_REG_SP,0x7000);
        m68k_set_reg(M68K_REG_PC,0x200);m68k_set_reg(M68K_REG_A0,0x6000);m68k_set_reg(M68K_REG_A2,0x6000);
        m68k_set_reg(M68K_REG_D0,lhs);m68k_set_reg(M68K_REG_D4,lhs);
        write(0x6000+(type&1?0:type==2?8:4),4,rhs);m68k_execute(1);
        unsigned expected=m68k_get_reg(nullptr,M68K_REG_SR)&31;
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);
        m68k_set_reg(M68K_REG_PC,0x1000);m68k_set_reg(M68K_REG_A1,0x9000);
        m68k_set_reg(M68K_REG_D1,rhs);
        m68k_set_reg(M68K_REG_D0,0x12345678); // clock bookkeeping clobbered scratch D0
        write(0x8000,4,lhs);write(0x8010,2,0x2500|flags);write(0x8012,4,0x123400);
        write(0x9004,4,rhs);write(0x9008,2,0x8000|type);
        unsigned before[16];for(unsigned r=0;r<16;++r)before[r]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r));
        unsigned steps=0;while(m68k_get_reg(nullptr,M68K_REG_PC)!=0x1000+length && steps++<40)m68k_execute(1);
        assert(steps<40);assert(read(0x8010,2)==(0x2500|expected));assert(read(0x8012,4)==0x123400);
        for(unsigned r=1;r<16;++r)assert(before[r]==m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r)));
        ++checks;
    }
    printf("PASS: %u assembled sentinel cases: all CCR combinations, signed-overflow boundaries, unchanged nonscratch registers and frame PC\n",checks);
    file=fopen(argv[2],"rb");assert(file);length=fread(memory.data()+0x1000,1,512,file);assert(feof(file) && length && length<512);fclose(file);
    unsigned decline=0x1000+std::strtoul(argv[3],nullptr,10);
    const unsigned bounds[]={0x20000,0x24000,0x30000,0x34000};
    for(unsigned i=0;i<4;++i)write(std::strtoul(argv[4+i],nullptr,10),4,bounds[i]);
    const unsigned bases[]={0,1,2,4,8,0x1fff8,0x1fffc,0x20000,0x20002,0x20004,0x23ff4,0x23ff8,0x23ffc,0x23ffd,0x23ffe,0x24000,0x2fff8,0x2fffc,0x30000,0x33ff4,0x33ff8,0x33ffc,0x33ffe,0x34000,0x50000,0xfffffff8,0xfffffffc,0xffffffff};
    checks=0;
    for(unsigned type=0;type<4;++type)for(unsigned base:bases){
        unsigned ea=base+(type&1?0:type==2?8:4);
        bool owned=(ea>=bounds[0] && ea<=bounds[1]-4) || (ea>=bounds[2] && ea<=bounds[3]-4);
        bool accepted=base==0 || (!(ea&1) && owned);
        if(owned)write(ea,4,0x89abcdef);
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_PC,0x1000);
        m68k_set_reg(M68K_REG_A0,0x123400);m68k_set_reg(M68K_REG_A1,0x9000);m68k_set_reg(M68K_REG_A2,base);
        m68k_set_reg(M68K_REG_D1,0xdeadbeef);
        write(0x8008,4,base);write(0x8012,4,0x123400);write(0x9004,4,0x12344321);write(0x9008,2,0x8000|type);
        unsigned steps=0,pc=0;
        while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=0x1000+length && pc!=decline && steps++<50)m68k_execute(1);
        assert(steps<50);assert((pc==0x1000+length)==accepted);
        if(accepted){assert(m68k_get_reg(nullptr,M68K_REG_D1)==(base?0x89abcdef:0x12344321));assert(m68k_get_reg(nullptr,M68K_REG_A0)==0x123400);}
        ++checks;
    }
    printf("PASS: %u assembled address guards: null vectors, ROM/RAM boundaries, odd pointers, unmapped space and wrapping addresses\n",checks);
    file=fopen(argv[8],"rb");assert(file);length=fread(memory.data()+0x1000,1,2048,file);assert(feof(file) && length && length<2048);fclose(file);
    unsigned admitted=0x1000+std::strtoul(argv[9],nullptr,10);
    decline=0x1000+std::strtoul(argv[10],nullptr,10);
    unsigned body=0x1000+std::strtoul(argv[11],nullptr,10),done=0x1000+std::strtoul(argv[12],nullptr,10);
    unsigned virtualSr=std::strtoul(argv[13],nullptr,10);
    checks=0;
    unsigned userSp=std::strtoul(argv[44],nullptr,10),superSp=std::strtoul(argv[45],nullptr,10);
    unsigned switchEnabled=std::strtoul(argv[46],nullptr,10);
    // Both physical CPU models, both rollout settings, every SR and both AND
    // masks: preserve the exact virtual supervisor/user stacks and CCR.
    for(unsigned cpu: {M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})
    for(unsigned enabled=0;enabled<2;++enabled)for(unsigned mask=0;mask<2;++mask){
    m68k_set_cpu_type(cpu);write(switchEnabled,2,enabled);
    for(unsigned type=0;type<3;++type)for(unsigned sr=0;sr<65536;++sr){
        unsigned old=type==2?0x2700:sr,physical=0x2500|(sr&31),operand=type==0?0x0500:mask?0xd0ff:0xf8ff;
        unsigned result=type==2?sr:type==0?(old|operand):(old&operand);
        bool accepted=(old&0x2000) && !(result&0x8000) && ((result&0x2000)||enabled);
        unsigned referenceSr=0,referenceSp=0,referenceSsp=0;
        if(accepted){
            // Independent original 68000 instruction, even when the native
            // implementation is tested on a physical 68020. Virtual frames
            // retain the original six-byte 68000 format.
            m68k_set_cpu_type(M68K_CPU_TYPE_68000);
            m68k_set_reg(M68K_REG_SR,old);m68k_set_reg(M68K_REG_SP,0x30600);
            m68k_set_reg(M68K_REG_USP,0x32000);m68k_set_reg(M68K_REG_PC,0x200);
            write(0x200,2,type==2?0x4e73:type==0?0x007c:0x027c);
            write(0x202,2,operand);write(0x30600,2,sr);write(0x30602,4,0x24044);
            m68k_execute(1);
            referenceSr=m68k_get_reg(nullptr,M68K_REG_SR);
            referenceSp=m68k_get_reg(nullptr,M68K_REG_SP);
            referenceSsp=m68k_get_reg(nullptr,M68K_REG_ISP);
            assert(referenceSr==(result&0xa71f));
            assert(m68k_get_reg(nullptr,M68K_REG_PC)==(type==2?0x24044:0x204));
        }
        m68k_set_cpu_type(cpu);
        write(userSp,4,0x32000);write(superSp,4,0x33000);
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_USP,0x30600);
        m68k_set_reg(M68K_REG_PC,0x1000);m68k_set_reg(M68K_REG_A1,0x9000);
        write(virtualSr,2,old);write(0x8010,2,physical);write(0x8012,4,0x23456);
        write(0x9004,4,operand);write(0x9008,2,0x4000|type);write(0x30600,2,sr);write(0x30602,4,0x24044);
        unsigned steps=0,pc;
        while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=admitted && pc!=decline && steps++<80)m68k_execute(1);
        assert(steps<80);assert((pc==admitted)==accepted);
        if(accepted){
            // Live clock work may alter scratch D0/A0 but preserves operand D1.
            m68k_set_reg(M68K_REG_PC,body);steps=0;
            while(m68k_get_reg(nullptr,M68K_REG_PC)!=done && steps++<40)m68k_execute(1);
            assert(steps<40);assert(read(virtualSr,2)==referenceSr);
            assert(read(0x8010,2)==((physical&~31)|(result&31)));
            assert(read(0x8012,4)==(type==2?0x24044:0x2345a));
            assert(m68k_get_reg(nullptr,M68K_REG_USP)==referenceSp);
            assert(read(superSp,4)==(!(result&0x2000)?referenceSsp:0x33000));
            assert(read(userSp,4)==0x32000);
        }else {assert(read(virtualSr,2)==old);assert(read(0x8012,4)==0x23456);}
        ++checks;
    }
    }
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    printf("PASS: %u assembled CPU-control cases: 68000/68020, all SR values, both transition settings/masks, CCR, PC and virtual stacks\n",checks);

    file=fopen(argv[14],"rb");assert(file);length=fread(memory.data()+0x1000,1,512,file);assert(feof(file));fclose(file);
    admitted=0x1000+std::strtoul(argv[15],nullptr,10);decline=0x1000+std::strtoul(argv[16],nullptr,10);
    file=fopen(argv[17],"rb");assert(file);length=fread(memory.data()+0x1800,1,512,file);assert(feof(file));fclose(file);
    done=0x1800+std::strtoul(argv[18],nullptr,10);
    for(unsigned which=0;which<2;++which){
        unsigned stub=std::strtoul(argv[19+which],nullptr,10);
        // Synthetic C ABI stubs deliberately clobber every volatile register.
        if(which){write(stub,2,0x2039);write(stub+2,4,0x9100);stub+=6;}
        else {write(stub,2,0x202f);write(stub+2,2,4);stub+=4;}
        write(stub,2,0x223c);write(stub+2,4,0xdeadbeef);stub+=6;
        write(stub,2,0x207c);write(stub+2,4,0xaaaaaaaa);stub+=6;
        write(stub,2,0x227c);write(stub+2,4,0xbbbbbbbb);stub+=6;
        write(stub,2,0x4e75);
    }
    checks=0;
    for(unsigned type=0;type<4;++type)for(unsigned flags=0;flags<32;++flags)for(unsigned value=0;value<256;++value){
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_PC,0x1000);
        m68k_set_reg(M68K_REG_A0,0x4000);m68k_set_reg(M68K_REG_A1,0x9000);m68k_set_reg(M68K_REG_A3,0x50000);
        m68k_set_reg(M68K_REG_A4,0x30608);m68k_set_reg(M68K_REG_A6,0x30604);
        m68k_set_reg(M68K_REG_D2,0xdeadbeef);m68k_set_reg(M68K_REG_D4,0xf00d0000|value);m68k_set_reg(M68K_REG_D5,0x1234fffb);
        write(0x4002,2,type==1?0xfffc:type==2?0x50fd:8);write(0x4004,2,8);
        write(0x9004,4,0x50008);write(0x9008,2,0x2000|type);write(0x9100,4,value);write(0x30600,1,value);
        write(0x8010,2,flags);write(0x8012,4,0x4000);
        unsigned steps=0,pc;
        while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=admitted && pc!=decline && steps++<80)m68k_execute(1);
        assert(steps<80 && pc==admitted);
        m68k_set_reg(M68K_REG_PC,0x1800);steps=0;
        while(m68k_get_reg(nullptr,M68K_REG_PC)!=done && steps++<80)m68k_execute(1);
        assert(steps<80);assert(read(0x8010,2)==((flags&16)|(value==0?4:0)|(value&128?8:0)));
        assert(read(0x8012,4)==(type==1 || type==2?0x4006:0x4004));
        assert(m68k_get_reg(nullptr,M68K_REG_D2)==(type==3?0xdeadbe00|value:0xdeadbeef));
        assert(m68k_get_reg(nullptr,M68K_REG_A1)==0x9000);assert(m68k_get_reg(nullptr,M68K_REG_SP)==0x8000);
        ++checks;
    }
    printf("PASS: %u assembled PIA MOVE cases: all byte values/CCR, signed source EAs, C ABI clobbers, D2 preservation and lengths\n",checks);

    file=fopen(argv[21],"rb");assert(file);length=fread(memory.data()+0x1000,1,512,file);assert(feof(file));fclose(file);
    admitted=0x1000+std::strtoul(argv[22],nullptr,10);decline=0x1000+std::strtoul(argv[23],nullptr,10);
    file=fopen(argv[24],"rb");assert(file);length=fread(memory.data()+0x1800,1,512,file);assert(feof(file));fclose(file);
    done=0x1800+std::strtoul(argv[25],nullptr,10);
    for(unsigned which=0;which<2;++which){
        unsigned stub=std::strtoul(argv[26+which],nullptr,10);
        if(which){write(stub,2,0x2039);write(stub+2,4,0x9100);stub+=6;}
        else {write(stub,2,0x202f);write(stub+2,2,8);stub+=4;}
        write(stub,2,0x223c);write(stub+2,4,0xdeadbeef);stub+=6;
        write(stub,2,0x207c);write(stub+2,4,0xaaaaaaaa);stub+=6;
        write(stub,2,0x227c);write(stub+2,4,0xbbbbbbbb);stub+=6;write(stub,2,0x4e75);
    }
    checks=0;
    for(unsigned form=0;form<13;++form)for(unsigned flags=0;flags<32;++flags)for(unsigned value=0;value<256;++value){
        bool writing=form>=6,immediate=form==12,indirect=form==3 || form==4 || form==5 || form==9 || form==10 || form==11;
        unsigned reg=form%3,kind=(writing?8:0)|(immediate?32:reg)|(indirect?64:0);
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_PC,0x1000);
        m68k_set_reg(M68K_REG_A0,0x4000);m68k_set_reg(M68K_REG_A1,0x9000);m68k_set_reg(M68K_REG_A3,0x50000);
        unsigned initial0=0x0bad0000|(writing?value:0x71),initial1=0x12340000|(writing?value:0x82),initial2=0xdeadbe00|(writing?value:0x93);
        m68k_set_reg(M68K_REG_D2,initial2);
        write(0x8000,4,initial0);write(0x8004,4,initial1);
        write(0x4002,2,immediate?value:8);write(0x4004,2,8);
        write(0x9004,4,0x50000+(indirect?0:8));write(0x9008,2,0x1000|kind);write(0x9100,4,value);
        write(0x8010,2,0x2500|flags);write(0x8012,4,0x4000);
        unsigned steps=0,pc;
        while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=admitted && pc!=decline && steps++<80)m68k_execute(1);
        assert(steps<80 && pc==admitted);
        m68k_set_reg(M68K_REG_PC,0x1800);steps=0;
        while(m68k_get_reg(nullptr,M68K_REG_PC)!=done && steps++<80)m68k_execute(1);
        assert(steps<80);assert(read(0x8010,2)==(0x2500|(flags&16)|(value==0?4:0)|(value&128?8:0)));
        assert(read(0x8012,4)==0x4000);assert(read(0x8000,4)==(!writing && reg==0?(initial0&~255)|value:initial0));assert(read(0x8004,4)==(!writing && reg==1?(initial1&~255)|value:initial1));
        assert(m68k_get_reg(nullptr,M68K_REG_D2)==(!writing && reg==2?(initial2&~255)|value:initial2));assert(m68k_get_reg(nullptr,M68K_REG_A1)==0x9000);assert(m68k_get_reg(nullptr,M68K_REG_SP)==0x8000);
        ++checks;
    }
    printf("PASS: %u assembled peripheral byte cases: register/immediate/indirect operands, all CCR/value combinations and C ABI clobbers\n",checks);

    file=fopen(argv[28],"rb");assert(file);length=fread(memory.data()+0x1000,1,512,file);assert(feof(file));fclose(file);
    admitted=0x1000+std::strtol(argv[29],nullptr,10);decline=0x1000+std::strtol(argv[30],nullptr,10);
    file=fopen(argv[31],"rb");assert(file);length=fread(memory.data()+0x1800,1,512,file);assert(feof(file));fclose(file);
    // The shared return label precedes the TRAP body in the native image.
    // Execute the body up to its final BRA rather than relocating that branch
    // over the start of synthetic memory.
    done=0x1800+length-4;
    assert(read(done,2)==0x6000);
    unsigned traps=std::strtoul(argv[33],nullptr,10),srAddress=std::strtoul(argv[13],nullptr,10);
    checks=0;
    unsigned userTrapFlag=std::strtoul(argv[47],nullptr,10);
    for(unsigned cpu: {M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})
    for(unsigned enabled=0;enabled<2;++enabled)for(unsigned supervisor=0;supervisor<2;++supervisor)
    for(unsigned number=0;number<16;++number)for(unsigned ipl=0;ipl<8;++ipl)for(unsigned flags=0;flags<32;++flags){
        unsigned sr=(supervisor?0x2000:0)|(ipl<<8)|flags;
        // Independent original 68000 TRAP: six-byte frame, correct stack bank.
        m68k_set_cpu_type(M68K_CPU_TYPE_68000);
        write((32+number)*4,4,0x2400);write(0x4200,2,0x4e40|number);
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x30800);
        m68k_set_reg(M68K_REG_USP,0x32000);m68k_set_reg(M68K_REG_SR,sr);
        m68k_set_reg(M68K_REG_PC,0x4200);expectedException=32+number;m68k_execute(1);assert(expectedException==0);
        unsigned expectedSr=read(0x307fa,2),expectedPc=read(0x307fc,4);
        unsigned expectedActiveSr=m68k_get_reg(nullptr,M68K_REG_SR);
        m68k_set_cpu_type(cpu);
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_USP,supervisor?0x30800:0x32000);
        m68k_set_reg(M68K_REG_PC,0x1000);m68k_set_reg(M68K_REG_D1,number);
        write(std::strtoul(argv[4],nullptr,10),4,0x2000);write(std::strtoul(argv[5],nullptr,10),4,0x10000);
        write(std::strtoul(argv[6],nullptr,10),4,0x30000);write(std::strtoul(argv[7],nullptr,10),4,0x40000);
        write(traps+number*32+4,4,0x2400);write(srAddress,2,sr&~31);
        write(superSp,4,0x30800);write(userSp,4,0x32100);write(userTrapFlag,2,enabled);
        write(0x307fa,2,0xbeef);write(0x307fc,4,0x12345678);
        write(0x8010,2,flags);write(0x8012,4,0x4202);
        unsigned steps=0,pc;
        while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=admitted && pc!=decline && steps++<80)m68k_execute(1);
        assert(steps<80 && (pc==admitted)==bool(supervisor||enabled));
        if(pc==decline){
            assert(read(0x307fa,2)==0xbeef && read(0x307fc,4)==0x12345678);
            assert(read(userSp,4)==0x32100 && read(superSp,4)==0x30800);
            assert(read(srAddress,2)==(sr&~31));++checks;continue;
        }
        m68k_set_reg(M68K_REG_PC,0x1800);steps=0;
        while(m68k_get_reg(nullptr,M68K_REG_PC)!=done && steps++<80)m68k_execute(1);
        assert(steps<80);assert(m68k_get_reg(nullptr,M68K_REG_USP)==0x307fa);
        assert(read(0x307fa,2)==expectedSr && read(0x307fc,4)==expectedPc);
        assert(read(srAddress,2)==expectedActiveSr);assert(read(0x8010,2)==flags);assert(read(0x8012,4)==0x2400);
        assert(read(userSp,4)==(supervisor?0x32100:0x32000));assert(read(superSp,4)==0x30800);
        assert(m68k_get_reg(nullptr,M68K_REG_SP)==0x8000);++checks;
    }
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);write(userTrapFlag,2,0);
    for(unsigned invalid=0;invalid<10;++invalid){
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_USP,0x30800);
        m68k_set_reg(M68K_REG_PC,0x1000);m68k_set_reg(M68K_REG_D1,0);
        write(srAddress,2,0x2000);write(0x8012,4,0x4202);write(0x4200,2,0x4e40);write(traps+4,4,0x2400);
        if(invalid<2)write(srAddress,2,invalid?0xa000:0);
        else if(invalid<5)m68k_set_reg(M68K_REG_USP,invalid==2?0x30004:invalid==3?0x40000:0x30801);
        else if(invalid==5)write(0x4200,2,0x4e41);
        else if(invalid==6)write(0x8012,4,0x1802);
        else write(traps+4,4,invalid==7?0x2401:invalid==8?0x10000:0x40000);
        unsigned steps=0,pc;
        while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=admitted && pc!=decline && steps++<80)m68k_execute(1);
        assert(steps<80 && pc==decline);++checks;
    }
    // User entry validates the saved supervisor stack, not the current user
    // pointer. Rejected frames/targets/trace must not modify either bank.
    for(unsigned cpu: {M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})
    for(unsigned invalid=0;invalid<10;++invalid){
        m68k_set_cpu_type(cpu);write(userTrapFlag,2,1);
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_USP,0x32000);
        m68k_set_reg(M68K_REG_PC,0x1000);m68k_set_reg(M68K_REG_D1,0);
        unsigned badStack[]={0,0x30004,0x30801,0x40000,0xfffffffe};
        unsigned savedSp=invalid<5?badStack[invalid]:0x30800;
        write(superSp,4,savedSp);write(userSp,4,0x32100);
        write(srAddress,2,invalid==5?0x8000:0);write(0x8012,4,invalid==6?0x1802:0x4202);
        write(0x4200,2,invalid==7?0x4e41:0x4e40);write(traps+4,4,invalid==8?0x2401:invalid==9?0x10000:0x2400);
        write(0x307fa,2,0xbeef);write(0x307fc,4,0x12345678);
        unsigned steps=0,pc;
        while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=admitted && pc!=decline && steps++<80)m68k_execute(1);
        assert(steps<80 && pc==decline);
        assert(read(superSp,4)==savedSp && read(userSp,4)==0x32100);
        assert(m68k_get_reg(nullptr,M68K_REG_USP)==0x32000);
        assert(read(0x307fa,2)==0xbeef && read(0x307fc,4)==0x12345678);++checks;
    }
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    printf("PASS: %u assembled TRAP cases: all vectors/IPL/CCR, independent CPU exception frames, stack/target/opcode/privilege guards\n",checks);

    file=fopen(argv[34],"rb");assert(file);length=fread(memory.data()+0x1000,1,512,file);assert(feof(file));fclose(file);
    admitted=0x1000+std::strtol(argv[35],nullptr,10);decline=0x1000+std::strtol(argv[36],nullptr,10);
    file=fopen(argv[37],"rb");assert(file);length=fread(memory.data()+0x1800,1,512,file);assert(feof(file));fclose(file);
    done=0x1800+std::strtol(argv[38],nullptr,10);
    unsigned helper=std::strtoul(argv[39],nullptr,10);
    unsigned selectorBody=0x1800+std::strtoul(argv[40],nullptr,10),selector=std::strtoul(argv[41],nullptr,10);
    unsigned inlineCount=std::strtoul(argv[42],nullptr,10),headerGrant=std::strtoul(argv[43],nullptr,10);
    write(selector,4,0x9500);write(selector+4,4,0x9501);write(selector+8,4,0x9502);
    for(unsigned i=0;i<4;++i)write(std::strtoul(argv[4+i],nullptr,10),4,bounds[i]);
    const unsigned videoKinds[]={0,1,2,3,7},videoOps[]={0x10bc,0x117c,0x30bc,0x317c,0x3159};
    checks=0;
    for(unsigned form=0;form<5;++form)for(unsigned mode=0;mode<(form?1u:5u);++mode)for(unsigned flags=0;flags<32;++flags)for(unsigned sample=0;sample<260;++sample){
        unsigned kind=videoKinds[form],size=kind&2?2:1;
        unsigned value=sample<256?sample:sample==256?0x7fff:sample==257?0x8000:sample==258?0xff00:0xffff;
        unsigned source=sample&1?0x20000:0x33ffe,port=0x50008,base=kind&1?port+8:port;
        unsigned instructionLength=(kind&1) && !(kind&4)?6:4;
        write(0x4000,2,videoOps[form]);write(0x4002,2,kind&4?0xfff8:value);write(0x4004,2,0xfff8);write(source,2,value);
        m68k_set_reg(M68K_REG_SR,0x2500|flags);m68k_set_reg(M68K_REG_SP,0x7000);m68k_set_reg(M68K_REG_PC,0x4000);
        m68k_set_reg(M68K_REG_A0,base);m68k_set_reg(M68K_REG_A1,source);m68k_execute(1);
        unsigned expectedFlags=m68k_get_reg(nullptr,M68K_REG_SR),expectedSource=m68k_get_reg(nullptr,M68K_REG_A1),expectedValue=read(port,size);
        assert(m68k_get_reg(nullptr,M68K_REG_PC)==0x4000+instructionLength);
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_PC,0x1000);
        m68k_set_reg(M68K_REG_A0,0x4000);m68k_set_reg(M68K_REG_A1,0x9000);
        write(0x8008,4,base);write(0x800c,4,source);write(0x8010,2,0x2500|flags);write(0x8012,4,0x4000);
        write(0x9004,4,port);write(0x9008,2,0x0800|kind);
        unsigned steps=0,pc;
        while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=admitted && pc!=decline && steps++<80)m68k_execute(1);
        assert(steps<80 && pc==admitted);
        write(0x9500,1,0xa5);write(0x9501,1,mode?((mode-1)&1):1);write(0x9502,1,mode?((mode-1)>>1):1);
        write(inlineCount,4,12);write(headerGrant,4,1);
        m68k_set_reg(M68K_REG_PC,mode?selectorBody:0x1800);steps=0;unsigned calls=0;
        while(m68k_get_reg(nullptr,M68K_REG_PC)!=done && steps++<80){
            if(m68k_get_reg(nullptr,M68K_REG_PC)==helper){
                unsigned sp=m68k_get_reg(nullptr,M68K_REG_SP);
                assert(read(sp+4,4)==port);assert((read(sp+8,4)&(size==1?255:65535))==expectedValue);assert(read(sp+12,4)==kind);
                m68k_set_reg(M68K_REG_D0,read(sp+8,4));m68k_set_reg(M68K_REG_D1,0xdeadbeef);
                m68k_set_reg(M68K_REG_A0,0xaaaaaaaa);m68k_set_reg(M68K_REG_A1,0xbbbbbbbb);
                m68k_set_reg(M68K_REG_PC,read(sp,4));m68k_set_reg(M68K_REG_SP,sp+4);++calls;
            }else m68k_execute(1);
        }
        assert(steps<80 && calls==unsigned(!mode));
        if(mode){assert(read(0x9500,1)==(expectedValue&255) && !read(0x9501,1) && !read(0x9502,1));assert(!read(inlineCount,4) && !read(headerGrant,4));}
        assert(read(0x8010,2)==expectedFlags);assert(read(0x8008,4)==base);assert(read(0x800c,4)==expectedSource);
        assert(read(0x8012,4)==0x4000);assert(m68k_get_reg(nullptr,M68K_REG_A1)==0x9000);assert(m68k_get_reg(nullptr,M68K_REG_SP)==0x8000);++checks;
    }
    for(unsigned source:bases)for(unsigned wrong=0;wrong<2;++wrong){
        bool accepted=!wrong && !(source&1) && ((source>=bounds[0] && source<=bounds[1]-2) || (source>=bounds[2] && source<=bounds[3]-2));
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_PC,0x1000);
        m68k_set_reg(M68K_REG_A0,0x4000);m68k_set_reg(M68K_REG_A1,0x9000);
        write(0x4002,2,2);write(0x8008,4,0x50000+wrong);write(0x800c,4,source);write(0x8012,4,0x4000);
        write(0x9004,4,0x50002);write(0x9008,2,0x0807);
        unsigned steps=0,pc;
        while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=admitted && pc!=decline && steps++<80)m68k_execute(1);
        assert(steps<80 && (pc==admitted)==accepted);assert(read(0x800c,4)==source);++checks;
    }
    printf("PASS: %u assembled video MOVE cases: independent CPU flags/operands, postincrement, byte/word writes, C ABI clobbers and invalid EA guards\n",checks);

}
