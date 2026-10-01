#ifndef POKERI_CARD_BACK_CACHE_H
#define POKERI_CARD_BACK_CACHE_H
#include "Hd63484.h"
namespace pokeri {
// The descriptor and bitmap are generated locally from the user's ROMs. This
// class contains only command matching, state reuse and observation ordering.
class CardBackCache {
public:
    CardBackCache()=default;
    ~CardBackCache(){detach();}
    CardBackCache(const CardBackCache&)=delete;
    CardBackCache &operator=(const CardBackCache&)=delete;
    enum {Width=88,Height=100,WhiteCommands=29,Commands=79,Words=260,BitmapWords=2800,MaxGuards=128};
    struct Recipe {const uint16_t *words,*offsets;const uint32_t *context;};
    // Pixel offset from the bottom-left blit origin; prepared once with the cache.
    struct Guard {uint16_t offset,allowed,rightWhite;};
    struct Progress {int16_t x,y;uint32_t scalarWork,rectangleWork;};
    // Build-local output of the same renderer proof passes, never a guest snapshot.
    struct Prepared {
        unsigned version;
        Recipe source;
        const uint16_t *image,*mask;
        const Guard *guards;
        const Progress *progress,*whiteProgress,*rightWhiteProgress;
        unsigned guardCount,coverage;
        bool whiteReady;
    };
    bool installPrepared(Recipe descriptor,const Prepared &prepared,uint16_t *imageStorage,uint16_t *maskStorage);

    // A borrowed view of authoritative state, not a second device model.
    // Revoke before any callback, device access, scheduler boundary or return
    // to guest execution. Only exact non-final completions are allowed; the
    // controls flag permits WPR (except RWP) and relative MOVE; absolute
    // additionally permits exact translated AMOVE after cache admission.
    struct RasterGrant {
        const uint16_t *words=nullptr,*offsets=nullptr;
        const Progress *progress=nullptr;
        // Optional capture destination, exercised by the kernel tests. Exact
        // recipe reconstruction makes it unnecessary for the production cache.
        uint16_t *buffered=nullptr,*pending=nullptr,*parameter=nullptr;
        unsigned *matched=nullptr,*used=nullptr,*pendingCount=nullptr;
        int *pendingLength=nullptr;
        uint8_t *writeHigh=nullptr,*status=nullptr;
        uint32_t *work=nullptr;
        bool *stopped=nullptr,*cpuTried=nullptr;
        uint16_t **cpuData=nullptr;
        Hd63484::CommandCount *commands=nullptr;
        int anchorX=0,anchorY=0;
        uint32_t origin=0,rectangleWork=0,controls=0,absolute=0;
    };
    bool rasterGrant(Hd63484 &video,RasterGrant &out,bool controls=false,bool absolute=false);
    bool prepare(Recipe descriptor,uint16_t *imageStorage,uint16_t *maskStorage);
    void attach(Hd63484 &video,bool rectangleSemantics);
    void detach();
    bool command(Hd63484 &video,const uint16_t *words,unsigned count);
    void flush(Hd63484 &video,unsigned reason=0);
    // Includes the initial register/move commands before pixels are deferred.
    bool sequenceIncoming()const{return matched!=0;}
#ifdef POKERI_TIME_LEDGER
    // Optional observation only; portable model does not own a platform clock.
    void (*timing)(unsigned kind,unsigned detail)=nullptr;
    void wordStart(uint16_t w){if(timing && !matched && w==recipe.words[0])timing(0,w);}
#endif
    bool ready=false,enabled=true,whiteReady=false,whiteEnabled=true;
    bool rightWhiteEnabled=true;
    const char *error=nullptr;
    uint32_t whiteHits=0;
    uint32_t starts=0,hits=0,misses=0,barriers=0,prefixReplays=0,guardMisses=0,contextMisses=0,boundsMisses=0;
    uint32_t mismatchStage[80]={},barrierStage[80]={},barrierReason[8]={};
    unsigned guardCount=0,coverage=0;
    Guard guards[MaxGuards];
    Progress progress[Commands],whiteProgress[Commands],rightWhiteProgress[Commands];
private:
    struct Entry {
        std::array<uint8_t,256> control;
        std::array<uint16_t,32> parameter;
        std::array<uint16_t,16> pattern;
        uint32_t origin,mask,rwp;
        uint8_t status;
    } entry;
    Recipe recipe{};
    uint16_t *image=nullptr,*mask=nullptr;
    unsigned matched=0,used=0;
    int anchorX=0,anchorY=0;
    uint32_t destination=0;
    bool rectangles=false;
    Hd63484 *owner=nullptr;
    // Small semantic context only: no packed VRAM allocation.
    Hd63484 shadow{false};
    // Exact recipe tracking can continue when its bitmap is unsafe to reuse.
    bool trackingOnly=false,whiteBackground=false,rightWhiteBackground=false;
    const Progress *selectedProgress()const{return rightWhiteBackground?rightWhiteProgress:whiteBackground?whiteProgress:progress;}
    bool context(const Hd63484 &v)const;
    bool admit(Hd63484 &v,int x,int y);
    void save(const Hd63484 &v);
    void restoreShadow(Surface *surface);
    void clear(){matched=used=0;trackingOnly=whiteBackground=rightWhiteBackground=false;}
};
}
#endif
