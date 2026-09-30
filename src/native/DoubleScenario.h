#pragma once
#include <cstdint>
namespace pokeri {
// External keyboard driver for native and isolated host diagnostics. Never supplies game
// state: like a player reading the screen, it holds a pair, four of a suit or
// else the highest dealt card; a losing hand starts another round, and only the
// ROM's Double-ready indication allows a Double press. Times are board cycles.
struct DoubleScenario {
    unsigned round=1,phase=0,held=0;
    uint32_t start=0,deadline=0;
    bool doubled=false,done=false,failed=false;
    static constexpr uint32_t ms(unsigned value){return uint32_t(value)*uint16_t(8000);}
    static bool due(uint32_t now,uint32_t when){return !(uint32_t(now-when)&0x80000000u);}
    // Hold selection from five dealt card rank and suit words.
    template<class Rank,class Suit> static unsigned holds(Rank rank,Suit suit){
        unsigned keep=0;
        for(unsigned i=0;i<5;++i)for(unsigned j=0;j<5;++j)
            if(i!=j && rank(i)==rank(j))keep|=1u<<i;
        if(keep)return keep;
        for(unsigned i=0;i<5;++i){
            unsigned mask=0,count=0;
            for(unsigned j=0;j<5;++j)if(suit(i)==suit(j)){mask|=1u<<j;++count;}
            if(count>=4)return mask;
        }
        // The ROM does not accept Draw with nothing held: keep the highest card.
        unsigned best=0;
        for(unsigned i=1;i<5;++i)if(rank(i)>rank(best))best=i;
        return 1u<<best;
    }
    template<class Holds,class Key> void step(uint32_t now,bool ready,Holds choose,Key key){
        if(done || failed)return;
        if(doubled){
            if(!due(now,deadline))return;
            if(phase==0){key(0x22,false);phase=1;deadline=now+ms(4000);}
            else if(phase==1){key(0x4f,true);phase=2;deadline=now+ms(200);}
            else if(phase==2){key(0x4f,false);phase=3;deadline=now+ms(8000);}
            else done=true;
            return;
        }
        // coin, deal, hold (chosen when pressed), draw
        static const uint16_t times[]={100,300,1500,1700,8500,8700,10500,10700};
        while(phase<8 && due(now,start+ms(times[phase]))){
            switch(phase++){
            case 0:key(0x33,true);break;
            case 1:key(0x33,false);break;
            case 2:case 6:key(0x40,true);break;
            case 3:case 7:key(0x40,false);break;
            case 4:held=choose();for(unsigned i=0;i<5;++i)if(held&(1u<<i))key(i+1,true);break;
            case 5:for(unsigned i=0;i<5;++i)if(held&(1u<<i))key(i+1,false);break;
            }
        }
        if(phase==8 && ready){
            key(0x22,true);doubled=true;phase=0;deadline=now+ms(200);
        }else if(due(now,start+ms(22000))){
            if(round==12){failed=true;return;}
            ++round;phase=0;start=now;
        }
    }
};
}
