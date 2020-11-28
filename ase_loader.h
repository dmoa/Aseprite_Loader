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
#include "decompressor.h"

uint16_t GetU16(char* memory) {
	uint8_t* p = (uint8_t*)(memory);
	return (((uint16_t)p[1]) << 8) |
		   (((uint16_t)p[0]));
}

uint32_t GetU32(void* memory) {
	uint8_t* p = (uint8_t*)(memory);
	return (((uint32_t)p[3]) << 24) |
		   (((uint32_t)p[2]) << 16) |
		   (((uint32_t)p[1]) <<  8) |
		   (((uint32_t)p[0]));
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
    uint32_t file_size;
    uint16_t magic_number;
    uint16_t num_frames;
    uint16_t width;
    uint16_t height;
    uint16_t color_depth;    // 32 RGBA, 16 Grayscale, 8 Indexed
    uint32_t flags;          // 1 = layer opacity valid
    uint16_t speed;          // frame speed, depricated
    uint8_t  palette_entry;  // the one transparent colour for indexed sprites only
    uint16_t num_colors;

    // Pixel ratio. Default 1:1.
    uint8_t  pixel_width;
    uint8_t  pixel_height;


    // Rendered grid for aseprite, not for asset loading.
    int16_t  x_grid;
    int16_t  y_grid;
    uint16_t grid_width;
    uint16_t grid_height;
};

struct Ase_Frame {
    uint32_t num_bytes;
    uint16_t magic_number;
    uint16_t old_num_chunks; // old specifier of number of chunks
    uint16_t frame_duration;
    uint32_t new_num_chunks; // number of chunks, if 0, use old field.
};

struct Ase_Tag {
    uint16_t from;
    uint16_t to;
    const char* name;
};

struct Palette_Entry {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

// Need to fix, for now assuming .ase are indexed sprites,
// but will need to change in the future as there won't always be 256 entries.
struct Palette_Chunk {
    uint32_t num_entries;
    Palette_Entry entry [256];
};

struct Ase_Output {
    int width;
    int height;
    uint8_t** const pixels;
    Palette_Chunk palette;
};

Ase_Output AseLoad() {

    std::ifstream file("example.ase", std::ifstream::binary);

    if (file) {

        file.seekg(0, file.end);
        const int length = file.tellg();
        file.seekg(0, std::ios::beg);

        char buffer [length];
        file.read(buffer, length);


        Ase_Header header = {
            GetU32(& buffer[0]),
            GetU16(& buffer[4]),
            GetU16(& buffer[6]),
            GetU16(& buffer[8]),
            GetU16(& buffer[10]),
            GetU16(& buffer[12]),
            GetU32(& buffer[14]),
            GetU16(& buffer[18]),
            (uint8_t) buffer[28],
            GetU16(& buffer[32]),
            (uint8_t) buffer[34],
            (uint8_t) buffer[35],
            (int16_t) GetU16(& buffer[36]),
            (int16_t) GetU16(& buffer[38]),
            GetU16(& buffer[40]),
            GetU16(& buffer[42])
        };

        Ase_Frame frames [header.num_frames];
        Palette_Chunk palette;
        uint8_t** const pixel_data = new uint8_t* [header.num_frames];
        for (int i = 0; i < header.num_frames; i++) pixel_data[i] = new uint8_t [header.width * header.height];
        const Ase_Tag** tags = NULL;

        Ase_Output output = {header.width, header.height, pixel_data, palette};

        char* buffer_p = & buffer[HEADER_SIZE];

        for (int i = 0; i < header.num_frames; i++) {

            // fill the pixel indexes in the frame with transparent color index
            for (int j = 0; j < header.width * header.height; j++) {
                pixel_data[i][j] = header.palette_entry;
            }

            frames[i] = {
                GetU32(buffer_p),
                GetU16(buffer_p + 4),
                GetU16(buffer_p + 6),
                GetU16(buffer_p + 8),
                GetU32(buffer_p + 12)
            };

            if (frames[i].magic_number != FRAME_MN) {
                std::cout << "Frame " << i << " magic number not correct, corrupt file?" << std::endl;
                exit(-1);
            }

            buffer_p += FRAME_SIZE;


            for (int j = 0; j < frames[i].new_num_chunks; j++) {

                uint32_t chunk_size = GetU32(buffer_p);
                uint16_t chunk_type = GetU16(buffer_p + 4);

                switch (chunk_type) {

                    case PALETTE: {

                        palette.num_entries = GetU32(buffer_p + 6);
                        // Specifies the range of unique colors in the palette.
                        // There may be many repeated colors, so range -> efficient.
                        uint32_t first_to_change = GetU32(buffer_p + 10);
                        uint32_t  last_to_change = GetU32(buffer_p + 14);

                        for (int k = first_to_change; k < last_to_change; k++) {

                            if (GetU16(buffer_p + 18) == 1) {
                                std::cout << "Name flag detected, cannot load! Color Index: " << k << std::endl;
                                exit(-1);
                            }
                            palette.entry[k] = {buffer_p[20 + k*6], buffer_p[22 + k*6], buffer_p[24 + k*6], buffer_p[26 + k*6]};
                        }
                        break;
                    }

                    case CEL: {
                        int16_t x = GetU16(buffer_p + 8);
                        int16_t y = GetU16(buffer_p + 10);
                        uint16_t cel_type = GetU16(buffer_p + 13);

                        if (cel_type != 2) {
                            std::cout << "Pixel format not supported! Exit.";
                            exit(-1);
                        }

                        uint16_t width  = GetU16(buffer_p + 22);
                        uint16_t height = GetU16(buffer_p + 24);
                        uint8_t pixels [width * height];

                        unsigned int data_size = Decompressor_Feed(buffer_p + 26, 26 - chunk_size, & pixels[0], width * height, true);
                        if (data_size == -1) {
                            std::cout << "Failed to decompress pixels! Exit." << std::endl;
                            exit(-1);
                        }

                        for (int k = 0; k < width * height; k ++) {
                            pixel_data[i][y * width + k] = pixels[k];
                        }

                        break;
                    }

                    default: break;
                }

                buffer_p += chunk_size;
            }


        }

        // std::cout << header.file_size << std::endl;
        // std::cout << header.magic_number << std::endl;
        // std::cout << header.num_frames << std::endl;
        // std::cout << header.width << std::endl;
        // std::cout << header.height << std::endl;
        // std::cout << header.color_depth << std::endl;
        // std::cout << header.flags << std::endl;
        // std::cout << header.speed << std::endl;
        // std::cout << (int64_t) header.palette_entry << std::endl;
        // std::cout << header.num_colors << std::endl;
        // std::cout << (int64_t) header.pixel_width << std::endl;
        // std::cout << (int64_t) header.pixel_height << std::endl;
        // std::cout << header.x_grid << std::endl;
        // std::cout << header.y_grid << std::endl;
        // std::cout << header.grid_width << std::endl;
        // std::cout << header.grid_height << std::endl;

        for (int i = 0; i < header.width * header.height; i++) {
            std::cout << (int) pixel_data[0][i] << " ";
        }

        return output;


    } else {
        std::cout << "file could not be loaded" << std::endl;
        exit(-1);
    }
}