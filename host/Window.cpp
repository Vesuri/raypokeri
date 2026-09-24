#include "Window.h"
#include <stdexcept>
#ifdef POKERI_SDL
#include <SDL.h>
Window::~Window(){SDL_DestroyTexture((SDL_Texture*)texture);SDL_DestroyRenderer((SDL_Renderer*)renderer);SDL_DestroyWindow((SDL_Window*)window);if(enabled)SDL_Quit();}
void Window::open(uint64_t cycle){
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS))throw std::runtime_error(SDL_GetError());
    window=SDL_CreateWindow("Pokeri research — experimental palette",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1152,584,SDL_WINDOW_RESIZABLE);
    if(!window)throw std::runtime_error(SDL_GetError());
    renderer=SDL_CreateRenderer((SDL_Window*)window,-1,0);if(!renderer)throw std::runtime_error(SDL_GetError());
    enabled=true;started=SDL_GetTicks64();startCycle=cycle;
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
bool Window::poll(pokeri::Board&){return true;}
void Window::show(const pokeri::VideoFrame&,uint64_t,unsigned){}
#endif
