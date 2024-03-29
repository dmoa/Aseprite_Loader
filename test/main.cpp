#pragma once

#include <iostream>

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

#define strequal(a,b) strcmp(a,b)==0

#define ASE_LOADER_IMPLEMENTATION
#include "../Ase_Loader/Ase_Loader.h"


// For now using SDL_Delay instead of this, but will probably use this later.
struct Clock {
    // dt
    float last_tick_time = 0;
    float dt = 0;

    void tick() {
        float tick_time = SDL_GetTicks();
        dt = (tick_time - last_tick_time) / 1000;
        last_tick_time = tick_time;
    }
};

struct Graphics {
    SDL_Window* window;
    SDL_Renderer* rdr;
};

enum Test_Types {
    NULL_TEST,
    SLICES,
    SLICE_NAMES
};

// Expected results
struct Test {
    Test_Types type = NULL_TEST;
    char* file_path = NULL;
    void* expected;
};

struct TestIter {
    Ase_Output* ase = NULL;
    int i = 0; // index
};


void StartIthTest(TestIter* test_iter, Test* tests);
void FinishIthTest(TestIter* test_iter);
void GraphicsLaunch(Graphics* g);
void GraphicsShutdown(Graphics* g);
bool EventLoop();


int main(int argc, char* argv[]) {

    bool graphics_mode = false;
    if (argc > 1) {
        if (strcmp(argv[1], "graphic") == 0) {
            graphics_mode = true;
        }
        else {
            print("%s", argv[1]);
            print("Invalid arg. Available: graphic");
        }
    }


    Test tests [] = {
        {SLICES, "tests/1_no_slices_blank.ase", 0},
        {SLICES, "tests/1.1_no_slices.ase", 0},
        {SLICES, "tests/2.1_no_slices.ase", 0},
        {SLICES, "tests/2.2_no_slices_animated.ase", 0},
        {SLICES, "tests/3.1_seven_slices_blank.ase", 7},
        {SLICES, "tests/3.0_one_slice.ase", 1},
        {SLICES, "tests/3.2_animated_two_slices.ase", 2},
        {SLICE_NAMES, "tests/4.0_slice_names_empty.ase", "example_slice"},
        {NULL_TEST, "tests/5.0_rgba_format.ase", "NULL"},
	};

    TestIter test_iter;
    int num_tests = sizeof(tests) / sizeof(tests[0]);

    //////// GRAPHICS MODE ////////
    if (graphics_mode) {

        Graphics g;
        GraphicsLaunch(& g);
        SDL_Texture* test_texture = NULL;

        for (; test_iter.i < num_tests; test_iter.i++) {

            StartIthTest(& test_iter, & tests[0]);

            SDL_PixelFormatEnum pixel_format;
            if (test_iter.ase->bpp == 1) {
                pixel_format = SDL_PIXELFORMAT_INDEX8;
            }
            else if (test_iter.ase->bpp == 4) {
                pixel_format = SDL_PIXELFORMAT_RGBA32;
            }
            else {
                print("Test %i | %i BPP not supported!", test_iter.i, test_iter.ase->bpp);
            }

            SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(test_iter.ase->pixels, test_iter.ase->frame_width * test_iter.ase->num_frames, test_iter.ase->frame_height, test_iter.ase->bpp * 8, test_iter.ase->bpp * test_iter.ase->frame_width * test_iter.ase->num_frames, pixel_format);
            if (! surface) print("Surface could not be created!, %s\n", SDL_GetError());
            SDL_SetPaletteColors(surface->format->palette, (SDL_Color*) & test_iter.ase->palette.entries, 0, test_iter.ase->palette.num_entries);
            SDL_SetColorKey(surface, SDL_TRUE, test_iter.ase->palette.color_key);

            test_texture = SDL_CreateTextureFromSurface(g.rdr, surface);

            FinishIthTest(& test_iter);

            // Draw the test texture if it actually exists
            // (checking != NULL on the off chance that a test failes miserably and the texture also fails to load)
            if (test_texture != NULL) {
                SDL_RenderClear(g.rdr);
                SDL_Rect drect = {0, 0, 0, 0};
                SDL_QueryTexture(test_texture, NULL, NULL, & drect.w, & drect.h);
                SDL_RenderCopy(g.rdr, test_texture, NULL, & drect);
                SDL_RenderPresent(g.rdr);

                // If we're on the last test, then keep the texture up on the screen.
                if (test_iter.i < num_tests - 1) SDL_DestroyTexture(test_texture);
            }

            // If we get an input that we want to quit the program, we do as so.
            if (EventLoop()) {
                SDL_DestroyTexture(test_texture);
                GraphicsShutdown(& g);
                return 0;
            }

            SDL_Delay(300);
        }

        // This lets us have the window open even after we finish all the tests.
        // This makes it easier to create the ability to cycle back through or pick
        // specific tests in the future.
        while (true) {
            if (EventLoop()) {
                SDL_DestroyTexture(test_texture);
                GraphicsShutdown(& g);
                break;
            }
        }
    }

    //////// NON-GRAPHICS MODE ////////
    else {
        for (; test_iter.i < num_tests; test_iter.i++) {
            StartIthTest(& test_iter, & tests[0]);
            FinishIthTest(& test_iter);
        }
    }

    return 0;
}

// Tests the ith test.
// The only thing it doesn't do is SDL_DestroyTexture, because when we are in graphics mode
// we want to wait until we draw the texture to the screen before we destroy it.
void StartIthTest(TestIter* test_iter, Test* tests) {

    test_iter->ase = Ase_Load(tests[test_iter->i].file_path);

    u32 expected = tests[test_iter->i].expected;
    u32 actual;
    bool success;

    if (tests[test_iter->i].type == SLICES) {
        actual = test_iter->ase->num_slices;
        success = expected == actual;
    }
    else if (tests[test_iter->i].type == SLICE_NAMES) {
        actual = test_iter->ase->slices[0].name;
        success = strequal(expected, actual);
    }
    else {
        print("Test %i has null test type!", test_iter->i);
        return;
    }


    if (success) {
        print("Test %i | %s | PASSED", test_iter->i, tests[test_iter->i].file_path);
    }
    else {
        print("Test %i | %s | FAILED | Expected %i, Got %i", test_iter->i, tests[test_iter->i].file_path, expected, actual);
    }

}

void FinishIthTest(TestIter* test_iter) {
    Ase_Destroy_Output(test_iter->ase);
}

void GraphicsLaunch(Graphics* g) {
    SDL_Init(SDL_INIT_EVERYTHING);
    IMG_Init(IMG_INIT_PNG);

    g->window = SDL_CreateWindow("Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1600, 800, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    g->rdr = SDL_CreateRenderer(g->window, -1, SDL_RENDERER_ACCELERATED);
    const int scale = 4;
    SDL_RenderSetScale(g->rdr, scale, scale);
}

void GraphicsShutdown(Graphics* g) {
    SDL_DestroyWindow(g->window);
    IMG_Quit();
    SDL_Quit();
}

// Returns true if quiting the program
bool EventLoop() {

    bool quit = false;

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

    return quit;
}