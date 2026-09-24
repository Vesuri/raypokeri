#include "Board.h"
#include <algorithm>
#include <cmath>
namespace pokeri {
// AY timing/gating cross-checked against MAME ay8910.cpp (BSD-3-Clause).
// This is a digital reference, without a measured board amplifier/load model.
void Ay38912::clockStep() {
    for(unsigned c=0;c<3;++c){
        unsigned period=std::max(1u,unsigned(registers[2*c]|(registers[2*c+1]<<8)));
        ++toneCount[c];while(toneCount[c]>=period){toneCount[c]-=period;toneHigh[c]=!toneHigh[c];}
    }
    if(++noiseCount>=std::max(1u,unsigned(registers[6]))){
        noiseCount=0;noiseHalf=!noiseHalf;
        if(!noiseHalf)lfsr=(lfsr>>1)|(((lfsr^(lfsr>>3))&1)<<16);
    }
    unsigned period=2*unsigned(registers[11]|(registers[12]<<8));
    if(!envelopeHold && ++envelopeCount>=std::max(1u,period)){
        envelopeCount=0;
        if(envelopeStep) --envelopeStep;
        else {
            unsigned shape=registers[13];
            if(!(shape&8)){envelopeHold=true;envelopeAttack=0;}
            else if(shape&1){if(shape&2)envelopeAttack^=15;envelopeHold=true;}
            else {if(shape&2)envelopeAttack^=15;envelopeStep=15;}
        }
    }
}
void Ay38912::tick(uint32_t cycles) {
    if(!clockHz)return;
    clockPhase+=uint64_t(cycles)*clockHz;
    while(clockPhase>=uint64_t(cpuHz)*8){
        clockPhase-=uint64_t(cpuHz)*8;clockStep();
        samplePhase+=uint64_t(sampleRate)*8;
        while(samplePhase>=clockHz){
            samplePhase-=clockHz;
            int raw=0;
            // Explicit approximate logarithmic DAC, 3 dB per step; no ROM data.
            static const auto levels=[](){std::array<int,16> a{};for(unsigned i=1;i<16;++i)a[i]=int(10000*std::pow(2.0,(int(i)-15)/2.0));return a;}();
            for(unsigned c=0;c<3;++c)if((toneHigh[c] || (registers[7]&(1<<c))) && ((lfsr&1) || (registers[7]&(8<<c)))){
                unsigned v=registers[8+c];raw+=levels[(v&16)?(envelopeStep^envelopeAttack):(v&15)];
            }
            dc+=(int64_t(raw)*65536-dc)/1024;
            if(sink)sink->sample(int16_t(raw-dc/65536));
        }
    }
}
void Ay38912::state(State &s) {
    s.fields(registers,writes,selected,port,clockHz,cpuHz,sampleRate,clockPhase,samplePhase,
             toneCount,noiseCount,envelopeCount,lfsr,toneHigh,noiseHalf,envelopeHold,envelopeStep,envelopeAttack,dc);
    if(!cpuHz || sampleRate!=44100 || envelopeStep>15 || envelopeAttack>15 || !lfsr || lfsr>0x1ffff)throw std::runtime_error("invalid AY state");
}
}
