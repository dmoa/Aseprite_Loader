#pragma once

uint8_t  GetU8(char* memory) {
    uint8_t* p = (uint8_t*)(memory);
    return p[0];
}

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
            GetU16(& buffer[18]), // speed + 2*DWORD. DWORD = 4 bytes. 2 * 4 + 2 = 10
            GetU8(& buffer[28]), // palette entry + BYTE*3
            GetU16(& buffer[32]), // num colors
            GetU8(& buffer[34]), // pixel width
            GetU8(& buffer[35]), // pixel height
            (int16_t) GetU16(& buffer[36]), // grid x
            (int16_t) GetU16(& buffer[38]), // grid y
            GetU16(& buffer[40]), // grid width
            GetU16(& buffer[42]) // grid height
        };

        std::cout << header.file_size << std::endl;
        std::cout << header.magic_number << std::endl;
        std::cout << header.num_frames << std::endl;
        std::cout << header.width << std::endl;
        std::cout << header.height << std::endl;
        std::cout << header.color_depth << std::endl;
        std::cout << header.flags << std::endl;
        std::cout << header.speed << std::endl;
        std::cout << header.palette_entry << std::endl;
        std::cout << header.num_colors << std::endl;
        std::cout << (int64_t)header.pixel_width << std::endl;
        std::cout << (int64_t) header.pixel_height << std::endl;
        std::cout << header.x_grid << std::endl;
        std::cout << header.y_grid << std::endl;
        std::cout << header.grid_width << std::endl;
        std::cout << header.grid_height << std::endl;


    } else {
        std::cout << "file could not be loaded" << std::endl;
    }
}