/*
Aseprite Loader
Copyright © 2020 Stan O

* Permission is granted to anyone to use this software
* for any purpose, including commercial applications,
* and to alter it and redistribute it freely, subject to
* the following restrictions:
*
* 1. The origin of this software must not be
*    misrepresented; you must not claim that you
*    wrote the original software. If you use this
*    software in a product, an acknowledgment in
*    the product documentation would be appreciated
*    but is not required.
*
* 2. Altered source versions must be plainly marked
*    as such, and must not be misrepresented as
*    being the original software.
*
* 3. This notice may not be removed or altered from
*    any source distribution.




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


- Only supports indexed color mode
- Only supports zlib compressed pixel data

- Does not support blend mode
- Does not support layers

Let me know if you want something added,
    ~ Stan

*/

#pragma once

#include <stdio.h>
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
    s16  x_grid;
    s16  y_grid;
    u16 grid_width;
    u16 grid_height;
};

struct Ase_Frame {
    u32 num_bytes;
    u16 magic_number;
    u16 old_num_chunks; // old specifier of number of chunks
    u16 frame_duration;
    u32 new_num_chunks; // number of chunks, if 0, use old field.
};

struct Ase_Tag {
    u16 from;
    u16 to;
    std::string name;
};

// delete and replace with SDL_Color if using SDL
struct Ase_Color {
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
    Ase_Color entries [256];
};

struct Rect {
    int x;
    int y;
    int w;
    int h;
};

struct Slice {
    std::string name;
    Rect quad;
};

struct Ase_Output {
    Ase_Output() {}; // c++ bullshit makes me define an empty constructor...
    u8* pixels;
    int frame_width;
    int frame_height;
    Palette_Chunk palette;

    Ase_Tag* tags;
    int num_tags;

    u16* frame_durations;
    int num_frames;

    Slice* slices;
    u32 num_slices;
};

inline void Ase_Destroy_Output(Ase_Output* output);

Ase_Output* Ase_Load(std::string path) {

    std::ifstream file(path, std::ifstream::binary);

    if (file) {

        file.seekg(0, file.end);
        const int length = file.tellg();
        file.seekg(0, std::ios::beg);

        char buffer [length];
        file.read(buffer, length);
        file.close();
        char* buffer_p = & buffer[HEADER_SIZE];

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

        Ase_Output* output = new Ase_Output();
        output->pixels = new u8 [header.width * header.height * header.num_frames];
        output->frame_width  = header.width;
        output->frame_height = header.height;
        output->palette.color_key = header.palette_entry;

        output->frame_durations = new u16 [header.num_frames];
        output->num_frames   = header.num_frames;

        // Aseprite doesn't tell us upfront how many slices we're given,
        // so there's no way really of creating the array of size X before
        // we receive all the slices. Vector is used temporarily, but then
        // converted into Slice* for output.
        std::vector<Slice> temp_slices;

        // helps us with formulating output but not all data needed for output
        Ase_Frame frames [header.num_frames];

        // fill the pixel indexes in the frame with transparent color index
        for (int i = 0; i < header.width * header.height * header.num_frames; i++) {
            output->pixels[i] = header.palette_entry;
        }

        for (int i = 0; i < header.num_frames; i++) {


            frames[i] = {
                GetU32(buffer_p),
                GetU16(buffer_p + 4),
                GetU16(buffer_p + 6),
                GetU16(buffer_p + 8),
                GetU32(buffer_p + 12)
            };

            output->frame_durations[i] = frames[i].frame_duration;

            if (frames[i].magic_number != FRAME_MN) {
                std::cout << "Frame " << i << " magic number not correct, corrupt file?" << std::endl;
                Ase_Destroy_Output(output);
                return NULL;
            }

            buffer_p += FRAME_SIZE;


            for (int j = 0; j < frames[i].new_num_chunks; j++) {

                u32 chunk_size = GetU32(buffer_p);
                u16 chunk_type = GetU16(buffer_p + 4);

                switch (chunk_type) {

                    case PALETTE: {

                        output->palette.num_entries = GetU32(buffer_p + 6);
                        // Specifies the range of unique colors in the palette.
                        // There may be many repeated colors, so range -> efficient.
                        u32 first_to_change = GetU32(buffer_p + 10);
                        u32  last_to_change = GetU32(buffer_p + 14);

                        for (int k = first_to_change; k < last_to_change; k++) {

                            if (GetU16(buffer_p + 26) == 1) {
                                std::cout << "Name flag detected, cannot load! Color Index: " << k << std::endl;
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

                        if (cel_type != 2) {
                            std::cout << "Pixel format not supported! Exit.\n";
                            Ase_Destroy_Output(output);
                            return NULL;
                        }

                        u16 width  = GetU16(buffer_p + 22);
                        u16 height = GetU16(buffer_p + 24);
                        u8 pixels [width * height];

                        unsigned int data_size = Decompressor_Feed(buffer_p + 26, 26 - chunk_size, pixels, width * height, true);
                        if (data_size == -1) {
                            std::cout << "Failed to decompress pixels! Exit.\n";
                            Ase_Destroy_Output(output);
                            return NULL;
                        }

                        const int pixel_offset = header.width * header.num_frames * y_offset + i * header.width + x_offset;

                        for (int k = 0; k < width * height; k ++) {
                            int index = pixel_offset + k%width + floor(k / width) * header.width * header.num_frames;
                            output->pixels[index] = pixels[k];
                        }

                        break;
                    }

                    case TAGS: {
                        int size = GetU16(buffer_p + 6);
                        output->tags = new Ase_Tag[size];

                        int tag_buffer_offset = 0;
                        for (int k = 0; k < size; k ++) {

                            output->tags[k].from = GetU16(buffer_p + tag_buffer_offset + 16);
                            output->tags[k].to = GetU16(buffer_p + tag_buffer_offset + 18);

                            int slen = GetU16(buffer_p + tag_buffer_offset + 33);
                            output->tags[k].name = "";
                            for (int a = 0; a < slen; a ++) {
                                output->tags[k].name += *(buffer_p + tag_buffer_offset + a + 35);
                            }

                            tag_buffer_offset += 19 + slen;
                        }
                        break;
                    }
                    case SLICE: {
                        u32 num_keys = GetU32(buffer_p + 6);
                        u32 flag = GetU32(buffer_p + 10);
                        if (flag != 0) {
                            std::cout << "Flag " << flag << " not supported! Asset: " << path;
                            Ase_Destroy_Output(output);
                            return NULL;
                        }

                        int slen = GetU16(buffer_p + 18);
                        std::string name = "";
                        for (int a = 0; a < slen; a++) {
                            name += *(buffer_p + 20 + a);
                        }

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

        output->slices = new Slice [temp_slices.size()];
        for (int i = 0; i < temp_slices.size(); i ++) {
            output->slices[i] = temp_slices[i];
        }
        output->num_slices = temp_slices.size();

        return output;


    } else {
        std::cout << "file could not be loaded" << std::endl;
        return NULL;
    }
}

inline void Ase_Destroy_Output(Ase_Output* output) {
    delete [] output->pixels;
    delete [] output->frame_durations;
    delete [] output->tags;
    delete [] output->slices;
    delete output;
}