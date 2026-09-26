#include "../src/board/Board.h"
#include "../amiga/generated/PaulaWaves.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>
int main()try{
    unsigned total=0;
    for(const auto &wave:paulaWaveEntries){
        pokeri::Ay38912 ay;
        ay.registers[0]=wave.tone;ay.registers[1]=wave.tone>>8;ay.registers[6]=wave.noise;
        std::vector<int> signal;
        for(unsigned i=0;i<wave.length*6;++i){
            signal.push_back((ay.lfsr&1) && (!wave.tone || ay.toneHigh[0])?1:-1);
            ay.clockStep();
        }
        if(wave.length&1 || wave.length<4096 || wave.offset+wave.length>sizeof(paulaWaveData))throw std::runtime_error("invalid bank bounds");
        if(wave.tone && (ay.toneHigh[0] || ay.toneCount[0]))throw std::runtime_error("tone loop does not close");
        double kernel[63],sum=0;
        for(unsigned k=0;k<63;++k){int x=int(k)-31;double w=0.54-0.46*std::cos(2*M_PI*k/62);
            kernel[k]=w*(x?std::sin(2*M_PI*9000/125000*x)/(M_PI*x):2.0*9000/125000);sum+=kernel[k];}
        for(unsigned i=0;i<wave.length;++i){
            double expected=0;for(int k=0;k<63;++k)expected+=kernel[k]*signal[(i*6+k+signal.size()-31)%signal.size()];
            int sample=std::lround(expected*127/sum);if(sample>127)sample=127;if(sample< -127)sample=-127;
            int actual=int8_t(paulaWaveData[wave.offset+i]);
            if(std::abs(actual-sample)>1)throw std::runtime_error("offline oscillator differs from filtered AY reference");
            ++total;
        }
        // DMA slices cover exactly the loop, including its shorter last block.
        unsigned offset=0,visited=0;while(offset<wave.length){unsigned n=std::min(256u,wave.length-offset);
            if(!n || (n&1))throw std::runtime_error("invalid DMA slice");offset+=n;visited+=n;}
        if(visited!=wave.length)throw std::runtime_error("DMA loop coverage");
    }
    printf("PASS: all 37 offline loops / %u samples match filtered AY reference; whole tone cycles and DMA tails\n",total);
}catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
