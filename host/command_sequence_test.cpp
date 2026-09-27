#include "../src/board/CommandSequenceObserver.h"
#include <cassert>
#include <cstdio>
using pokeri::CommandSequenceObserver;
// Synthetic recipe, deliberately unrelated to original graphics.
static const uint16_t words[]={0x0800,0x2222,0x8000,3,4,0xcc00,0x8000,8,9,0xcc00};
static const uint16_t offsets[]={0,2,5,6,9,10};
static void feed(CommandSequenceObserver &o,unsigned c,int x=0,int y=0){
    uint16_t w[3];unsigned n=offsets[c+1]-offsets[c];
    for(unsigned i=0;i<n;++i)w[i]=words[offsets[c]+i];
    if(w[0]==0x8000){w[1]+=x;w[2]+=y;}
    o.command(w,n,true);
}
int main(){
    for(unsigned barrier=0;barrier<=5;++barrier){
        CommandSequenceObserver o(words,offsets,5,2);
        for(unsigned c=0;c<5;++c){if(c==barrier)o.observe();feed(o,c,-12,29);}
        o.observe();
        assert(o.starts==1 && o.complete==1 && !o.mismatches && !o.matched);
        assert(o.unobserved==(barrier<=2 || barrier==5?1u:0u));
    }
    for(unsigned c=0;c<5;++c)for(unsigned at=offsets[c];at<offsets[c+1];++at){
        CommandSequenceObserver o(words,offsets,5,2);
        for(unsigned j=0;j<c;++j)feed(o,j);
        uint16_t w[3];unsigned n=offsets[c+1]-offsets[c];
        for(unsigned j=0;j<n;++j)w[j]=words[offsets[c]+j];
        w[at-offsets[c]]^=1;
        o.command(w,n,true);
        for(unsigned j=c+1;j<5;++j)feed(o,j);
        // Both AMOVEs must share their translation, including the first anchor.
        assert(!o.complete);
        for(unsigned j=0;j<5;++j)feed(o,j);
        assert(o.complete==1);
    }
    CommandSequenceObserver o(words,offsets,5,2);
    for(unsigned c=0;c<4;++c)feed(o,c);
    o.command(words+9,1,false);assert(o.mismatches==1 && !o.complete);
    std::puts("command sequence observer: translation, mismatch restart, failure and observation boundaries pass");
}
