#ifndef POKERI_HOST_ACRTC_TIMING_DEVICE_H
#define POKERI_HOST_ACRTC_TIMING_DEVICE_H
#include "acrtc_timing_fifo.h"
#include "../src/board/Hd63484.h"
namespace pokeri_research {
// Research adapter around the unchanged authoritative renderer. Deliberately
// not a production Device replacement until duration policies are measured.
class AcrtcTimingDevice : public pokeri::Device {
public:
    using Duration=bool (*)(const pokeri::Hd63484&,const std::vector<uint16_t>&,uint64_t&);
private:
    struct Execution {
        pokeri::Hd63484 &video;
        Duration estimate;
        int format(uint16_t op)const{
            const auto &f=pokeri::Hd63484::formats[op>>10];
            return (op&f.reserved)?0:f.words;
        }
        bool duration(const std::vector<uint16_t>&w,uint64_t &ticks){return estimate && estimate(video,w,ticks);}
        bool complete(const std::vector<uint16_t>&w,uint64_t){
            // Do not select an address through write8: that would incorrectly
            // cancel an in-progress read-FIFO byte while an unrelated command
            // finishes. Only the adapter feeds the renderer's write FIFO.
            uint8_t address=video.ar;video.ar=0;
            for(uint16_t word:w)if(!video.writeFifoWord(word)){video.ar=address;return false;}
            video.ar=address;return !video.error;
        }
    } execution;
    AcrtcTimingFifo<Execution> fifo;
    const char *localError=nullptr;
    bool unavailable(){
        if(execution.video.presentationBusy){localError="ACRTC timing: shuffle presentation policy is not integrated";}
        return fault()!=nullptr;
    }
    bool drawingRegister(unsigned address)const{
        if(address==2)return true;
        for(unsigned n=0;n<4;++n)if(address==0xc2+8*n || address==0xc3+8*n)return true;
        return false;
    }
public:
    AcrtcTimingDevice(pokeri::Hd63484 &v,Duration d):execution{v,d},fifo(execution){
        if(v.receivingCommand() || v.cardCache || v.surface || v.wptnCountsBytes)
            localError="ACRTC timing: incompatible initial renderer context";
    }
    const char *fault()const{return localError?localError:execution.video.error?execution.video.error:fifo.error;}
    uint64_t clock()const{return fifo.now;}
    uint64_t completed()const{return fifo.completed;}
    uint8_t status()const{
        uint8_t chip=execution.video.statusNow();
        return uint8_t((chip & ~(pokeri::Hd63484::WFE|pokeri::Hd63484::WFR|pokeri::Hd63484::CED))|fifo.status()|(fault()?pokeri::Hd63484::CER:0));
    }
    uint8_t read8(unsigned offset)override{
        if(unavailable())return 0;
        return offset&2?execution.video.read8(offset):status();
    }
    void write8(unsigned offset,uint8_t value)override{
        if(unavailable())return;
        auto &v=execution.video;
        if(!(offset&2)){fifo.addressSelected();v.write8(offset,value);return;}
        if(v.ar<2){fifo.write8(value);return;}
        bool abort=v.ar==2 && (value&0x80);
        if(!abort && fifo.busyTicks() && drawingRegister(v.ar) && v.control[v.ar]!=value){
            localError="ACRTC timing: drawing-context write during execution needs latch semantics";return;
        }
        v.write8(offset,value);
        if(abort)fifo.abort();
    }
    void tick(uint32_t ticks)override{if(!unavailable())fifo.tick(ticks);}
    bool irq()const override{return (status()&execution.video.control[3])!=0;}
};
}
#endif
