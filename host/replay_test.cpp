#include "Replay.h"
#include "../src/native/Replay.h"
#include <vector>
#include <cassert>
using namespace pokeri;
int main(){
    ReplayWriter writer;writer.open("tmp/replay-synthetic.bin");
    writer.event(ReplayConfig,0,0,0,8000000);
    writer.event(ReplayBus,128,65536,0x1234);
    writer.event(ReplayIrq,128,65536,0x1238,5,0x43);
    writer.event(ReplayInput,130,66000,0x5678,0x10001,0xff);
    writer.event(ReplayEnd,1000,1000000,0x2000,1);writer.close();
    FILE*f=fopen("tmp/replay-synthetic.bin","rb");assert(f);std::vector<uint8_t> bytes;int ch;while((ch=fgetc(f))!=EOF)bytes.push_back(ch);fclose(f);
    ReplayReader reader(bytes.data(),bytes.size());ReplayEvent event{};
    assert(reader.next(event) && event.kind==ReplayConfig && event.a==8000000);
    assert(reader.next(event) && event.instruction==128 && event.cycle==65536 && event.pc==0x1234);
    assert(reader.next(event) && event.kind==ReplayIrq && event.a==5 && event.b==0x43);
    assert(reader.next(event) && event.kind==ReplayInput && event.a==0x10001 && event.b==0xff);
    assert(reader.next(event) && event.kind==ReplayEnd && reader.complete());assert(!reader.next(event));
    for(size_t length=0;length<bytes.size();++length){ReplayReader r(bytes.data(),length);while(r.next(event)){}assert(!r.complete());}
    auto bad=bytes;bad.push_back(0);ReplayReader trailing(bad.data(),bad.size());while(trailing.next(event)){}assert(!trailing.complete());
    bad=bytes;bad[8]=3;ReplayReader version(bad.data(),bad.size());assert(!version.next(event));
    bad=bytes;for(unsigned i=9;i<14;++i)bad[i]=255;ReplayReader overflow(bad.data(),bad.size());assert(!overflow.next(event));
    puts("PASS replay round trip, simultaneous events, version, truncation, trailing data and varint overflow");
}
