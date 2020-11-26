#pragma once

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

void AseLoad() {
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

        char* buffer_p = & buffer[HEADER_SIZE];

        for (int i = 0; i < header.num_frames; i++) {

            frames[i] = {
                GetU32(buffer_p),
                GetU16(buffer_p + 4),
                GetU16(buffer_p + 6),
                GetU16(buffer_p + 8),
                GetU32(buffer_p + 12)
            };

            if (frames[i].magic_number != FRAME_MN) {
                std::cout << "Frame " << i << " magic number not correct, corrupt file?" << std::endl;
                return;
            }

            buffer_p += FRAME_SIZE;


            for (int j = 0; j < frames[i].new_num_chunks; j++) {

                uint32_t chunk_size = GetU32(buffer_p);
                uint16_t chunk_type = GetU16(buffer_p + 4);

                switch (chunk_type) {

                    case OLD_PALETTE_1: case OLD_PALETTE_2: case MASK: case PATH:
                        std::cout << "deprictated chunk type" << std::endl;
                        break;

                    case PALETTE: {

                        palette.num_entries = GetU32(buffer_p + 6);
                        // the range of colors that are different
                        uint32_t first_to_change = GetU32(buffer_p + 10);
                        uint32_t  last_to_change = GetU32(buffer_p + 14);

                        for (int k = first_to_change; k < last_to_change; k++) {

                            if (GetU16(buffer_p + 18) == 1) {
                                std::cout << "Name flag detected, cannot load! Color Index: " << k << std::endl;
                                return;
                            }
                            palette.entry[k] = {buffer_p[20 + k*6], buffer_p[22 + k*6], buffer_p[24 + k*6], buffer_p[26 + k*6]};
                        }
                        break;
                    }

                    default:
                        std::cout << "type not accounted" << std::endl;
                        break;
                }

                buffer_p += chunk_size;
            }

        }


    } else {
        std::cout << "file could not be loaded" << std::endl;
    }
}