#pragma once

#include <stdio.h>
#include <time.h>
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


SDL_Renderer* w_renderer;
inline void DrawTexture(SDL_Texture* texture) {
    SDL_Rect drect = {0, 0, 0, 0};
    SDL_QueryTexture(texture, NULL, NULL, & drect.w, & drect.h);
    SDL_RenderCopy(w_renderer, texture, NULL, & drect);
}

SDL_Texture* LoadTest(std::string str) {
    Ase_Output* o = Ase_Load(str);
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(o->pixels, o->frame_width * o->num_frames, o->frame_height, 8, o->frame_width * o->num_frames, SDL_PIXELFORMAT_INDEX8);
    if (! surface) print("Surface could not be created!, %s\n", SDL_GetError());
    SDL_SetPaletteColors(surface->format->palette, (SDL_Color*) & o->palette.entries, 0, o->palette.num_entries);
    SDL_SetColorKey(surface, SDL_TRUE, o->palette.color_key);

    SDL_Texture* texture = SDL_CreateTextureFromSurface(w_renderer, surface);
    if (! texture) print("Texture could not be created!, %s:");
    SDL_FreeSurface(surface);

    return texture;
}

int main(int argc, char* argv[]) {

    SDL_Init(SDL_INIT_EVERYTHING);
    IMG_Init(IMG_INIT_PNG);

    SDL_Window* window = SDL_CreateWindow("Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1600, 800, SDL_WINDOW_SHOWN);
    w_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    const int scale = 4;
    SDL_RenderSetScale(w_renderer, scale, scale);

    SDL_Texture* test_texture = LoadTest("test.ase");

    SDL_Event event;
    bool quit = false;
    while (!quit) {

        while (SDL_PollEvent(& event)) {

            switch (event.type) {
                case SDL_QUIT:
                    quit = true;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            quit = true;
                            break;
                        case SDLK_r:
                            SDL_DestroyTexture(test_texture);
                            test_texture = LoadTest("test.ase");
                            break;
                        default: break;
                    }
                default: break;
            }
        }

        SDL_RenderClear(w_renderer);
        DrawTexture(test_texture);
        SDL_RenderPresent(w_renderer);
    }

    SDL_DestroyTexture(test_texture);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}