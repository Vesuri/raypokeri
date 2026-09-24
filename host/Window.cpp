#include "Window.h"
#include <stdexcept>
#ifdef POKERI_SDL
#include <SDL.h>
Window::~Window(){if(audioDevice)SDL_CloseAudioDevice(audioDevice);SDL_DestroyTexture((SDL_Texture*)texture);SDL_DestroyRenderer((SDL_Renderer*)renderer);SDL_DestroyWindow((SDL_Window*)window);if(enabled)SDL_Quit();}
void Window::open(uint64_t cycle){
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS))throw std::runtime_error(SDL_GetError());
    window=SDL_CreateWindow("Pokeri research — experimental palette",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1152,584,SDL_WINDOW_RESIZABLE);
    if(!window)throw std::runtime_error(SDL_GetError());
    renderer=SDL_CreateRenderer((SDL_Window*)window,-1,0);if(!renderer)throw std::runtime_error(SDL_GetError());
    enabled=true;started=SDL_GetTicks64();startCycle=cycle;
}
void Window::openAudio(){
    if(!enabled)throw std::runtime_error("live audio requires --window");
    if(SDL_InitSubSystem(SDL_INIT_AUDIO))throw std::runtime_error(SDL_GetError());
    SDL_AudioSpec requested{};
    requested.freq=44100;requested.format=AUDIO_S16SYS;requested.channels=1;requested.samples=512;
    // SDL converts to the hardware format if necessary; our queue remains AY PCM.
    audioDevice=SDL_OpenAudioDevice(nullptr,0,&requested,nullptr,0);
    if(!audioDevice)throw std::runtime_error(SDL_GetError());
}
void Window::sample(int16_t value){
    if(!audioDevice)return;
    audioBuffer[audioCount++]=value;
    if(audioCount==882)flushAudio();
}
static void waitAudio(unsigned device,unsigned maximum){
    uint64_t started=SDL_GetTicks64();
    while(SDL_GetQueuedAudioSize(device)>maximum){
        if(SDL_GetAudioDeviceStatus(device)!=SDL_AUDIO_PLAYING || SDL_GetTicks64()-started>1000)
            throw std::runtime_error("audio device stopped consuming samples");
        SDL_Delay(1);
    }
}
void Window::flushAudio(){
    if(!audioDevice || !audioCount)return;
    // Backpressure bounds latency even during boot, before a display exists.
    // Only wall time waits: no samples or emulated cycles are dropped.
    waitAudio(audioDevice,4410); // 50 ms PCM
    if(SDL_QueueAudio(audioDevice,audioBuffer,audioCount*sizeof(int16_t)))throw std::runtime_error(SDL_GetError());
    audioCount=0;SDL_PauseAudioDevice(audioDevice,0);
}
void Window::finishAudio(){
    flushAudio();
    if(audioDevice){
        waitAudio(audioDevice,0);
        SDL_CloseAudioDevice(audioDevice);audioDevice=0;
    }
}
bool Window::poll(pokeri::Board &b){
    if(!enabled)return true;
    SDL_Event e;
    while(SDL_PollEvent(&e)){
        if(e.type==SDL_QUIT)return false;
        if(e.type!=SDL_KEYDOWN && e.type!=SDL_KEYUP)continue;
        if(e.key.repeat)continue;
        bool down=e.type==SDL_KEYDOWN;auto key=e.key.keysym.sym;
        if(key==SDLK_ESCAPE && down)return false;
        // Active-low buttons, derived from the ROM's physical/logical map.
        unsigned side=0,bit=8;
        switch(key){
        case SDLK_SPACE:bit=0;break; // deal/draw
        case SDLK_RETURN:bit=1;break; // collect
        case SDLK_b:bit=2;break;
        case SDLK_LEFT:bit=3;break;
        case SDLK_RIGHT:bit=4;break;
        case SDLK_d:bit=5;break;
        case SDLK_5:bit=6;break;
        case SDLK_4:bit=7;break;
        case SDLK_3:side=1;bit=0;break;
        case SDLK_2:side=1;bit=1;break;
        case SDLK_1:side=1;bit=5;break;
        case SDLK_F1:if(down)b.pia[1].input[1]^=0x40;break; // cabinet door
        case SDLK_F2:side=1;bit=2;break; // rising-edge service button
        case SDLK_c:if(down){if(!b.peer.enabled)throw std::runtime_error("coin key requires --serial-peer");b.peer.enqueue({3});}break;
        default:break;
        }
        if(bit<8){if(down)b.pia[1].input[side]&=~(1<<bit);else b.pia[1].input[side]|=1<<bit;}
    }
    return true;
}
void Window::show(const pokeri::VideoFrame &f,uint64_t cycle,unsigned cpuHz){
    if(!enabled || !f.width || !f.height)return;
    if(f.width!=width || f.height!=height){
        SDL_DestroyTexture((SDL_Texture*)texture);width=f.width;height=f.height;
        texture=SDL_CreateTexture((SDL_Renderer*)renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,width,height);
        if(!texture)throw std::runtime_error(SDL_GetError());SDL_RenderSetLogicalSize((SDL_Renderer*)renderer,width,height);
    }
    std::vector<uint32_t> rgb;rgb.reserve(f.indices.size());for(auto i:f.indices)rgb.push_back(0xff000000|frameColor(i));
    if(SDL_UpdateTexture((SDL_Texture*)texture,nullptr,rgb.data(),width*4))throw std::runtime_error(SDL_GetError());
    SDL_RenderClear((SDL_Renderer*)renderer);SDL_RenderCopy((SDL_Renderer*)renderer,(SDL_Texture*)texture,nullptr,nullptr);SDL_RenderPresent((SDL_Renderer*)renderer);
    uint64_t due=(cycle-startCycle)*1000/cpuHz,elapsed=SDL_GetTicks64()-started;
    if(due>elapsed)SDL_Delay(unsigned(std::min<uint64_t>(due-elapsed,20)));
}
#else
Window::~Window(){}
void Window::open(uint64_t){throw std::runtime_error("window requires make harness SDL=1 and build/pokeri-host-sdl");}
void Window::openAudio(){throw std::runtime_error("live audio requires the SDL build");}
void Window::sample(int16_t){}
void Window::flushAudio(){}
void Window::finishAudio(){}
bool Window::poll(pokeri::Board&){return true;}
void Window::show(const pokeri::VideoFrame&,uint64_t,unsigned){}
#endif
