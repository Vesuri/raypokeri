// Uses SDL's dummy video driver; no ROMs, window or audible output.
#include "Window.h"
#include <SDL.h>
#include <cassert>
#include <cstdio>
#include <dlfcn.h>
static bool failRenderer=true;
extern "C" SDL_Renderer *pokeriTestCreateRenderer(SDL_Window *window,int index,Uint32 flags){
    if(failRenderer){SDL_SetError("injected renderer allocation failure");return nullptr;}
    typedef SDL_Renderer *(*Create)(SDL_Window*,int,Uint32);
    Create create=(Create)dlsym(RTLD_DEFAULT,"SDL_CreateRenderer");assert(create);
    return create(window,index,flags);
}
#include <stdexcept>
int main(){
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    // Fail after SDL_Init and window allocation have both succeeded.
    bool failed=false;
    {
        Window window;
        try{window.open(0);}catch(const std::runtime_error&){failed=true;}
        assert(failed && window.initialized && window.window && !window.renderer);
    }
    assert(SDL_WasInit(0)==0);
    failRenderer=false;
    SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
    {
        Window window;window.open(0);window.openAudio();
        assert(window.enabled && window.audioDevice);
    }
    assert(SDL_WasInit(0)==0);
    puts("PASS SDL partial startup and normal audio/window shutdown");
}
