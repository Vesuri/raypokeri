#include "AmigaInput.h"
#include "CabinetInput.h"
#include <proto/exec.h>
#include <proto/cia.h>
#include <resources/cia.h>
#include <hardware/cia.h>
#include <exec/interrupts.h>
#include "AmigaHardware.h"
static Library *ciaBase=nullptr;
static Interrupt keyboardInterrupt;
static Interrupt *savedKeyboard=nullptr;
static volatile uint8_t keys[128]={},pressed[128]={};
static bool installed=false,lamps=false;
static pokeri::CabinetInput cabinetInput;
void amigaInputObserve(uint32_t pc,const pokeri::Board &b){cabinetInput.observe(pc,b);}
void amigaInputKey(unsigned code,bool down){
    if(code>=128)return;
    if(down && !keys[code])pressed[code]=1;
    keys[code]=down;
}
static uint32_t keyboard(){
    uint8_t serial=*ciaasdrPointer;
    *ciaacraPointer|=CIACRAF_SPMODE;
    // Same conservative handshake as ROF: >85 us on a 68000.
    for(volatile uint16_t n=0;n<200;++n){}
    *ciaacraPointer&=uint8_t(~CIACRAF_SPMODE);
    uint8_t code=~serial;code=(code>>1)|(code<<7);
    amigaInputKey(code&127,!(code&128));return 0;
}
bool amigaInputStart(){
    ciaBase=(Library*)OpenResource((UBYTE*)CIAANAME);if(!ciaBase)return false;
    keyboardInterrupt.is_Node.ln_Type=NT_INTERRUPT;
    keyboardInterrupt.is_Node.ln_Name=(char*)"Pokeri keyboard";
    keyboardInterrupt.is_Code=(void(*)())keyboard;
    savedKeyboard=AddICRVector(ciaBase,CIAICRB_SP,&keyboardInterrupt);
    if(savedKeyboard){RemICRVector(ciaBase,CIAICRB_SP,savedKeyboard);
        if(AddICRVector(ciaBase,CIAICRB_SP,&keyboardInterrupt)){
            AddICRVector(ciaBase,CIAICRB_SP,savedKeyboard);savedKeyboard=nullptr;ciaBase=nullptr;return false;}}
    installed=true;return true;
}
void amigaInputStop(){
    if(installed){RemICRVector(ciaBase,CIAICRB_SP,&keyboardInterrupt);
        if(savedKeyboard)AddICRVector(ciaBase,CIAICRB_SP,savedKeyboard);}
    installed=false;ciaBase=nullptr;savedKeyboard=nullptr;
}
bool amigaInputQuit(){return keys[0x45]!=0;}
bool amigaInputLamps(){return lamps;}
void amigaInputApply(pokeri::Board &b){
    uint8_t edges[128],held[128];
    // Services now allow keyboard IRQs. Consume edges atomically so a new
    // press cannot be erased between reading the latch and clearing it.
    Disable();
    for(unsigned i=0;i<128;++i){edges[i]=pressed[i];held[i]=keys[i];pressed[i]=0;}
    Enable();
    auto down=[&](unsigned code){return held[code]||edges[code];};
    unsigned pa=0,pb=0;
    static const uint8_t paKeys[]={0x40,0x44,0x35,0x4f,0x4e,0x22,0x05,0x04};
    for(unsigned bit=0;bit<8;++bit)if(down(paKeys[bit]))pa|=1<<bit;
    if(down(1))pb|=0x20;if(down(2))pb|=2;if(down(3))pb|=1;if(down(0x51))pb|=4;
    // Joystick port 1: fire=Deal, up=Bet, down=Collect, left/right=Big/Small.
    unsigned joy=*joy1datPointer;
    if(!(*ciaapraPointer&0x80))pa|=1;
    if(joy&0x200)pa|=8;if(joy&2)pa|=16;
    if(((joy>>8)^(joy>>9))&1)pa|=4;if((joy^(joy>>1))&1)pa|=2;
    b.pia[1].input[0]=uint8_t(~pa);
    b.pia[1].input[1]=(b.pia[1].input[1]&~0x27)|uint8_t((~pb)&0x27);
    if(edges[0x50])cabinetInput.door();
    if(edges[0x33])cabinetInput.coin();
    cabinetInput.step(b);
    if(edges[0x52])lamps=!lamps;
}
