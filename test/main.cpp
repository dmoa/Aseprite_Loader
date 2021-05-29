#pragma once

#include <stdio.h>
#include <time.h>
#include <thread>
#include <string>
#include <SDL2/SDL.h>

#ifdef _WIN32
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#else
#include <SDL2_image/SDL_image.h>
#include <SDL2_ttf/SDL_ttf.h>
#endif

#define print SDL_Log
// In order to view Ase_Loader.h logs
#undef printf
#define printf SDL_Log

#include "../Ase_Loader/Ase_Loader.h"

struct EngineClock {
    // dt
    float last_tick_time = 0;

    // for average fps
    float fpss [ACCURACY];
    int fpss_index = -1;
    int average_fps = -1;

    void tick() {
        float tick_time = SDL_GetTicks();
        g_dt = (tick_time - last_tick_time) / 1000;
        last_tick_time = tick_time;
    }
};

struct Graphics {
    SDL_Window* window;
    SDL_Renderer* rdr;
};

struct Test {
    SDL_Texture* texture = NULL;
    Ase_Output* ase_output = NULL;
    // For when we're in graphics mode
    bool quit = false;
};


void GraphicsLaunch();
void GraphicsShutdown();
bool WindowThread();


int main(int argc, char* argv[]) {

    print("%i", argc);
    print(argv[0]);

    bool graphics_mode = false;

    if (argc > 1) {
        if (strcmp(argv[1], "graphic") != 0) {
            graphics_mode = true;
        }
        else {
            print("Invalid arg. Available: graphic");
        }
    }

    Test test_data;

    if (graphics_mode) {

        std::thread test_thread(Test, & test_data);

        Graphics g;
        GraphicsLaunch(& g);
        GraphicsLoop(& g, & test_data, & test_thread);
    }
    else {
        Test(& test_data);
    }

    return 0;
}

void Test(Test* test_data) {
    while (true) {
        if (test_data->quit) {
            print("exiting test loop!");
            break;
        }
    }
}

void GraphicsLaunch(Graphics* g) {
    SDL_Init(SDL_INIT_EVERYTHING);
    IMG_Init(IMG_INIT_PNG);

    g->window = SDL_CreateWindow("Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1600, 800, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    g->rdr = SDL_CreateRenderer(g->window, -1, SDL_RENDERER_ACCELERATED);
    const int scale = 4;
    SDL_RenderSetScale(g->rdr, scale, scale);
}

void GraphicsShutdown() {
    SDL_DestroyTexture(test_texture);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}

void GraphicsLoop(Graphics* g, Test* test_data) {

    bool quit = false;

    while (! quit) {

        SDL_Event event;
        while (SDL_PollEvent(& event)) {

            switch (event.type) {
                case SDL_QUIT:
                    quit = true;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            quit = true;
                            break;
                        default: break;
                    }
                default: break;
            }
        }

        // Draw the test texture if it actually exists
        if (test_data->texture != NULL) {
            SDL_RenderClear(g->rdr);
            SDL_Rect drect = {0, 0, 0, 0};
            SDL_QueryTexture(test_data->texture, NULL, NULL, & drect.w, & drect.h);
            SDL_RenderCopy(g->rdr, test_data->texture, NULL, & drect);
        }
    }

    // If we exit the program before the tests are done, there's a chance that we'll exit while there's a texture that hasn't been freed.
    // If we free the texture here and it's already been freed, SDL ignores it (the null pointer) and moves on :D.
    SDL_DestroyTexture(test_data->texture);
    Ase_Destroy_Output(test_data->ase_output);

    GraphicsShutdown();
}