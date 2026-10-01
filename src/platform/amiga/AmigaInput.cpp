#include "AmigaInput.h"
#include "CabinetInput.h"
#include "ReadLatchedButtons.h"
#include "AmigaKeyEvents.h"
#include <proto/exec.h>
#include <proto/cia.h>
#include <resources/cia.h>
#include <hardware/cia.h>
#include <exec/interrupts.h>
#include "AmigaHardware.h"
alignas(4) static Library *ciaBase=nullptr;
static Interrupt keyboardInterrupt;
static Interrupt *savedKeyboard=nullptr;
static pokeri::AmigaKeyEvents keyEvents;
alignas(4) static pokeri::ReadLatchedButtons buttons[2];
static void inputRead(unsigned side,uint8_t value,uint8_t mask){buttons[side].read(value,mask);}
static bool installed=false,lamps=false;
static pokeri::CabinetInput cabinetInput;
void amigaInputObserve(uint32_t pc,const pokeri::Board &b){cabinetInput.observe(pc,b);}
void amigaInputKey(unsigned code,bool down){keyEvents.key(code,down);}
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
    // This static object outlives the emergency heap sweep in main(). Release
    // its retained queue buffer now so its later destructor cannot free it twice.
    std::deque<uint8_t>().swap(cabinetInput.pending);
    cabinetInput.waitingDoor=cabinetInput.doorPass=false;
}
bool amigaInputQuit(){return keyEvents.quit();}
bool amigaInputLamps(){return lamps;}
void amigaInputApply(pokeri::Board &b){
    pokeri::AmigaKeyEvents::Snapshot events;
    // Consume only mapped events atomically; IRQs may produce the next batch
    // as soon as this bounded snapshot is complete.
    Disable();
    bool okay=keyEvents.take(events);
    Enable();
    b.inputRead=inputRead;
    okay=events.append(buttons) && okay;
    if(!okay){b.fault=true;b.faultReason="keyboard transition queue overflow";return;}
    unsigned pa=buttons[0].advance(),pb=buttons[1].advance();
    // Joystick port 1: fire=Deal, up=Bet, down=Collect, left/right=Big/Small.
    unsigned joy=*joy1datPointer;
    if(!(*ciaapraPointer&0x80))pa|=1;
    if(joy&0x200)pa|=8;if(joy&2)pa|=16;
    if(((joy>>8)^(joy>>9))&1)pa|=4;if((joy^(joy>>1))&1)pa|=2;
    b.pia[1].input[0]=uint8_t(~pa);
    b.pia[1].input[1]=(b.pia[1].input[1]&~0x27)|uint8_t((~pb)&0x27);
    for(unsigned i=0;i<events.count[pokeri::AmigaKeyEvents::Door];++i)cabinetInput.door();
    for(unsigned i=0;i<events.count[pokeri::AmigaKeyEvents::Coin];++i)cabinetInput.coin();
    cabinetInput.step(b);
    if(events.count[pokeri::AmigaKeyEvents::Lamps]&1)lamps=!lamps;
}
