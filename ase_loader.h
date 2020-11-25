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

struct Chunk_Format {
    uint32_t size;
    uint16_t chunk_type;
};

void AseLoad() {
    std::ifstream file("example.ase", std::ifstream::binary);

    if (file) {

        file.seekg(0, file.end);
        const int length = file.tellg();
        file.seekg(0, std::ios::beg);

        printf("size: %i\n", length);

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

        char* buffer_p = & buffer[128];

        for (int i = 0; i < header.num_frames; i++) {
            frames[i] = {
                GetU32(buffer_p),
                GetU16(buffer_p + 5),
                GetU16(buffer_p + 6),
                GetU16(buffer_p + 8),
                GetU32(buffer_p + 12)
            };

            if (frames[i].magic_number != FRAME_MN) {
                std::cout << "Frame " << i << " magic number not correct, corrupt file?" << std::endl;
            }

            buffer_p += 16;

            for (int j = 0; j < frames[i].new_num_chunks; j++) {

                GetU32(buffer_p);
                GetU16(buffer_p + 4);
                buffer_p += GetU32(buffer_p);
            }

        }

        std::cout << header.file_size << std::endl;
        std::cout << header.magic_number << std::endl;
        std::cout << header.num_frames << std::endl;
        std::cout << header.width << std::endl;
        std::cout << header.height << std::endl;
        std::cout << header.color_depth << std::endl;
        std::cout << header.flags << std::endl;
        std::cout << header.speed << std::endl;
        std::cout << (int64_t) header.palette_entry << std::endl;
        std::cout << header.num_colors << std::endl;
        std::cout << (int64_t) header.pixel_width << std::endl;
        std::cout << (int64_t) header.pixel_height << std::endl;
        std::cout << header.x_grid << std::endl;
        std::cout << header.y_grid << std::endl;
        std::cout << header.grid_width << std::endl;
        std::cout << header.grid_height << std::endl;

        for (int i = 0; i < header.num_frames; i++) {

            std::cout << "--------------------" << std::endl;
            std::cout << (int) frames[i].num_bytes << std::endl;
            std::cout << (int) frames[i].magic_number << std::endl;
            std::cout << (int) frames[i].old_num_chunks << std::endl;
            std::cout << (int) frames[i].frame_duration << std::endl;
            std::cout << (int) frames[i].new_num_chunks << std::endl;

        }


    } else {
        std::cout << "file could not be loaded" << std::endl;
    }
}