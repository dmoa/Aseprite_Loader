/*
Aseprite Loader
Copyright © 2020 Stan O

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Parses:
    - All header data
    - All frame data
    - All pixel data

Chunks Supported:
    - CEL
        - No opacity support
    - PALETTE 0x2019
        - No name support
    - SLICE 0x2022
        - Does not support 9 patches or pivot flags
        - Loads only first slice key

Types have Ase_ prefix if they're Ase specific.


- Only supports zlib compressed pixel data

- Does not support blend mode
- Does not support layers

Let me know if you want something added,
    ~ Stan

*/

#pragma once

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <math.h>
#include <stdint.h>
#include "decompressor.h"

typedef int64_t  s64;
typedef int32_t  s32;
typedef int16_t  s16;
typedef int8_t   s8;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

inline u16 GetU16(char* memory) {
	u8* p = (u8*)(memory);
	return (((u16)p[1]) << 8) |
		   (((u16)p[0]));
}

inline u32 GetU32(void* memory) {
	u8* p = (u8*)(memory);
	return (((u32)p[3]) << 24) |
		   (((u32)p[2]) << 16) |
		   (((u32)p[1]) <<  8) |
		   (((u32)p[0]));
}

#define bmalloc(t) (t*)(malloc(sizeof(t)))
#define bmalloc_arr(t,n) (t*)(malloc(sizeof(t)*n))
#define bcalloc_arr(t,n) (t*)(calloc(n, sizeof(t)))

#define HEADER_MN 0xA5E0
#define FRAME_MN 0xF1FA

#define HEADER_SIZE 128
#define FRAME_SIZE 16

#define OLD_PALETTE_1 0x0004 // depricated
#define OLD_PALETTE_2 0x0011 // depricated
#define LAYER 0x2004
#define CEL 0x2005
#define CEL_EXTRA 0x2006
#define COLOR_PROFILE 0x2007
#define MASK 0x2016 // depricated
#define PATH 0x2017 // depricated
#define TAGS 0x2018
#define PALETTE 0x2019
#define USER_DATA 0x2020
#define SLICE 0x2022

struct Ase_Header {
    u32 file_size;
    u16 magic_number;
    u16 num_frames;
    u16 width;
    u16 height;
    u16 color_depth;    // 32 RGBA, 16 Grayscale, 8 Indexed
    u32 flags;          // 1 = layer opacity valid
    u16 speed;          // frame speed, depricated
    u8  palette_entry;  // the one transparent colour for indexed sprites only
    u16 num_colors;

    // Pixel ratio. Default 1:1.
    u8  pixel_width;
    u8  pixel_height;


    // Rendered grid for aseprite, not for asset loading.
    s16 x_grid;
    s16 y_grid;
    u16 grid_width;
    u16 grid_height;
};


#ifdef ASE_LOADER_DEBUG

inline void Debug_PrintHeader(Ase_Header* h) {
    printf("file_size: %i\n", h->file_size);
    printf("magic_number: %i\n", h->magic_number);
    printf("num_frames: %i\n", h->num_frames);
    printf("width: %i\n", h->width);
    printf("height: %i\n", h->height);
    printf("color_depth: %i\n", h->color_depth);
    printf("flags: %i\n", h->flags);
    printf("speed: %i\n", h->speed);
    printf("palette_entry: %i\n", h->palette_entry);
    printf("num_colors: %i\n", h->num_colors);
    printf("pixel_width: %i\n", h->pixel_width);
    printf("pixel_height: %i\n", h->pixel_height);
}

#endif



struct Ase_Frame {
    u32 num_bytes;
    u16 magic_number;
    u16 old_num_chunks; // old specifier of number of chunks
    u16 frame_duration;
    u32 new_num_chunks; // number of chunks, if 0, use old field.
};

struct Animation_Tag {
    char* name;
    u16 from;
    u16 to;
};

// Delete and replace with SDL_Color if using SDL.
struct Color {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
};

// Need to fix, for now assuming .ase are indexed sprites,
// but will need to change in the future as there won't always be 256 entries.
struct Palette_Chunk {
    u32 num_entries;
    u8 color_key;
    Color entries [256];
};

// Delete and replace with SDL_Rect if using SDL
struct Rect {
    u32 x;
    u32 y;
    u32 w;
    u32 h;
};

struct Slice {
    char* name;
    Rect quad;
};

struct Ase_Output {
    u8* pixels;
    u8 bpp;           // bytes per pixel
    u16 frame_width;
    u16 frame_height;

    // Junk values if indexed color mode is not used
    Palette_Chunk palette;

    Animation_Tag* tags;
    u16 num_tags;

    u16* frame_durations;
    u16 num_frames;

    Slice* slices;
    u32 num_slices;
};



Ase_Output* Ase_Load(std::string path);
void Ase_Destroy_Output(Ase_Output* output);
bool Ase_SetFlipVerticallyOnLoad(bool input_flag);





#ifdef ASE_LOADER_IMPLEMENTATION





static bool vertically_flip_on_load = false;

bool Ase_SetFlipVerticallyOnLoad(bool input_flag) {
   vertically_flip_on_load = input_flag;
}


Ase_Output* Ase_Load(std::string path) {

    std::ifstream file(path, std::ifstream::binary);

    if (file) {

        file.seekg(0, file.end);
        const int file_size = file.tellg();

        char buffer [file_size];
        char* buffer_p = & buffer[HEADER_SIZE];

        // transfer data from file into buffer and close file
        file.seekg(0, std::ios::beg);
        file.read(buffer, file_size);
        file.close();

        Ase_Header header = {
            GetU32(& buffer[0]),
            GetU16(& buffer[4]),
            GetU16(& buffer[6]),
            GetU16(& buffer[8]),
            GetU16(& buffer[10]),
            GetU16(& buffer[12]),
            GetU32(& buffer[14]),
            GetU16(& buffer[18]),
            (u8) buffer[28],
            GetU16(& buffer[32]),
            (u8) buffer[34],
            (u8) buffer[35],
            (s16) GetU16(& buffer[36]),
            (s16) GetU16(& buffer[38]),
            GetU16(& buffer[40]),
            GetU16(& buffer[42])
        };

        if (! (header.color_depth == 8 || header.color_depth == 32)) {
            printf("%s: Color depth %i not supported.\n", path.c_str(), header.color_depth);
            return NULL;
        }

        Ase_Output* output = bmalloc(Ase_Output);
        output->bpp = header.color_depth / 8;
        output->pixels = bcalloc_arr(u8, header.width * header.height * header.num_frames * output->bpp); // using calloc instead of malloc so that we avoid junk pixel data
        output->frame_width = header.width;
        output->frame_height = header.height;
        output->palette.color_key = header.palette_entry;

        output->frame_durations = bmalloc_arr(u16, header.num_frames);
        output->num_frames = header.num_frames;


        // Because we are using malloc, we cannot use default values in struct because
        // the memory that we are given has garbage values, so we have to manually set
        // the values here.
        output->tags = NULL;
        output->num_tags = 0;
        output->slices = NULL;
        output->num_slices = 0;

        // Aseprite doesn't tell us upfront how many slices we're given,
        // so there's no way really of creating the array of size X before
        // we receive all the slices. Vector is used temporarily, but then
        // converted into Slice* for output.
        std::vector<Slice> temp_slices;

        // This helps us with formulating output but not all frame data is needed for output.
        Ase_Frame frames [header.num_frames];

        // Indexed? fill the pixel indexes in the frame with transparent color index
        if (header.color_depth == 8) {
            for (int i = 0; i < header.width * header.height * header.num_frames; i++) {
                output->pixels[i] = header.palette_entry;
            }
        }

        // Each frame may have multiple chunks, so we first get frame data, then iterate over all the chunks that the frame has.
        for (u16 current_frame_index = 0; current_frame_index < header.num_frames; current_frame_index++) {

            frames[current_frame_index] = {
                GetU32(buffer_p),
                GetU16(buffer_p + 4),
                GetU16(buffer_p + 6),
                GetU16(buffer_p + 8),
                GetU32(buffer_p + 12)
            };
            output->frame_durations[current_frame_index] = frames[current_frame_index].frame_duration;

            if (frames[current_frame_index].magic_number != FRAME_MN) {
                printf("%s: Frame %i magic number not correct, corrupt file?\n", path.c_str(), current_frame_index);
                Ase_Destroy_Output(output);
                return NULL;
            }

            buffer_p += FRAME_SIZE;

            for (u32 j = 0; j < frames[current_frame_index].new_num_chunks; j++) {

                u32 chunk_size = GetU32(buffer_p);
                u16 chunk_type = GetU16(buffer_p + 4);

                switch (chunk_type) {

                    case PALETTE: {

                        output->palette.num_entries = GetU32(buffer_p + 6);
                        // specifies the range of unique colors in the palette
                        // There may be many repeated colors, so range -> efficient.
                        u32 first_to_change = GetU32(buffer_p + 10);
                        u32  last_to_change = GetU32(buffer_p + 14);

                        for (u32 k = first_to_change; k < last_to_change + 1; k++) {

                            // We do not support color data with strings in it. Flag 1 means there's a name.
                            if (GetU16(buffer_p + 26) == 1) {
                                printf("%s: Name flag detected, cannot load! Color Index: %i.\n", path.c_str(), k);
                                Ase_Destroy_Output(output);
                                return NULL;
                            }
                            output->palette.entries[k] = {buffer_p[28 + k*6], buffer_p[29 + k*6], buffer_p[30 + k*6], buffer_p[31 + k*6]};
                        }
                        break;
                    }

                    case CEL: {

                        s16 x_offset = GetU16(buffer_p + 8);
                        s16 y_offset = GetU16(buffer_p + 10);
                        u16 cel_type = GetU16(buffer_p + 13);

                        if (x_offset < 0 || y_offset < 0) {
                            print("%s: Starting pixel coordinates out of bounds! Aseprite: Sprite -> Canvas Size -> Trim content outside of canvas [ON]", path.c_str());
                            return NULL;
                        }


                        if (cel_type != 2) {
                            printf("%s: Only compressed images supported\n", path.c_str());
                            Ase_Destroy_Output(output);
                            return NULL;
                        }

                        u16 width  = GetU16(buffer_p + 22);
                        u16 height = GetU16(buffer_p + 24);
                        u8 pixels [width * height * output->bpp];

                        // have to use pixels instead of output->pixels because we need to convert the pixel position if there's more than one frame
                        unsigned int data_size = Decompressor_Feed(buffer_p + 26, 26 - chunk_size, pixels, width * height * output->bpp, true);
                        if (data_size == -1) {
                            printf("%s: Pixel format not supported!\n", path.c_str());
                            Ase_Destroy_Output(output);
                            return NULL;
                        }

                        //
                        // transforming array of pixels onto larger array of pixels
                        //

                        // Our offset will be larger for a spritesheet because the total width of the image would have increased (when creating a spritesheet texture from .ase).
                        // Same logic with offset_x.
                        const int byte_offset_y = header.width * header.num_frames * y_offset * output->bpp;
                        const int byte_offset_x = (current_frame_index * header.width + x_offset) * output->bpp;
                        const int byte_offset = byte_offset_x + byte_offset_y;

                        const int total_num_bytes = width * height * output->bpp;

                        for (int k = 0; k < total_num_bytes; k ++) {
                            int index = byte_offset + k % (width * output->bpp) + floor(k / width / output->bpp) * header.width * header.num_frames * output->bpp;
                            output->pixels[index] = pixels[k];
                        }

                        break;
                    }

                    case TAGS: {

                        output->num_tags = GetU16(buffer_p + 6);;
                        output->tags = bmalloc_arr(Animation_Tag, output->num_tags);

                        // iterate over each tag and append data to output->tags
                        int tag_buffer_offset = 0;
                        for (u16 k = 0; k < output->num_tags; k ++) {

                            output->tags[k].from = GetU16(buffer_p + tag_buffer_offset + 16);
                            output->tags[k].to = GetU16(buffer_p + tag_buffer_offset + 18);

                            // get string
                            u16 slen = GetU16(buffer_p + tag_buffer_offset + 33);
                            output->tags[k].name = (char*) malloc(sizeof(char) * (slen + 1)); // slen + 1 because we need to make it a null terminating string

                            for (u16 a = 0; a < slen; a++) {
                                output->tags[k].name[a] = *(buffer_p + tag_buffer_offset + a + 35);
                            }
                            output->tags[k].name[slen] = '\0';

                            tag_buffer_offset += 19 + slen;
                        }
                        break;
                    }
                    case SLICE: {

                        u32 num_keys = GetU32(buffer_p + 6);
                        u32 flag = GetU32(buffer_p + 10);
                        if (flag != 0) {
                            printf("%s: Flag %i not supported!\n", path.c_str(), flag);
                            Ase_Destroy_Output(output);
                            return NULL;
                        }

                        // get string
                        u16 slen = GetU16(buffer_p + 18);
                        char* name = bmalloc_arr(char, slen + 1);

                        for (u16 a = 0; a < slen; a++) {
                            name[a] = *(buffer_p + a + 20);
                        }
                        name[slen] = '\0';

                        // For now, we assume that the slice is the same
                        // throughout all the frames, so we don't care about
                        // the starting frame_number.
                        // int frame_number = GetU32(buffer_p + 20 + slen);

                        Rect quad = {
                            (s32) GetU32(buffer_p + slen + 24),
                            (s32) GetU32(buffer_p + slen + 28),
                            GetU32(buffer_p + slen + 32),
                            GetU32(buffer_p + slen + 36)
                        };

                        temp_slices.push_back({name, quad});

                        break;
                    }
                    default: break;
                }
                buffer_p += chunk_size;
            }
        }

        // flip pixels if vertically_flip_on_load is true
        if (vertically_flip_on_load) {

            u8 temp; // temp variable for swapping
            int num_bytes_per_row = output->frame_width * output->num_frames * output->bpp;

            for (int i = 0; i < (int) output->frame_height / 2; i++) {

                // the pointers of the two rows we're swapping
                u8* swap_a = output->pixels + i * num_bytes_per_row;
                u8* swap_b = output->pixels + (output->frame_height - i - 1) * num_bytes_per_row;

                // Swapping two rows of pixels, pixel by pixel.
                // stb_image.h did it pixel by pixel instead of memcpy-ing the entire row at once
                // I'm going to trust that that's a wise move and do that as well.
                for (int j = 0; j < num_bytes_per_row; j++) {
                    temp = *(swap_a + j);
                    *(swap_a + j) = *(swap_b + j);
                    *(swap_b + j) = temp;
                }

            }
        }

        // convert vector to array for output

        output->slices = bmalloc_arr(Slice, temp_slices.size());

        // We do "int i" instead of u8 / u16 / u32... because we don't know how many slices there are upfront :(
        for (int i = 0; i < temp_slices.size(); i ++) {
            output->slices[i] = temp_slices[i];
        }
        output->num_slices = temp_slices.size();

        return output;


    } else {
        printf("%s: File could not be loaded.\n", path.c_str());
        return NULL;
    }
}

inline void Ase_Destroy_Output(Ase_Output* output) {

    free(output->pixels);
    free(output->frame_durations);

    for (int i = 0; i < output->num_tags; i++) {
        free(output->tags[i].name);
    }
    for (int i = 0; i < output->num_slices; i++) {
        free(output->slices[i].name);
    }

    // There are cases where memory is never allocated for these fyi.
    free(output->tags);
    free(output->slices);

    free(output);
}


#endif