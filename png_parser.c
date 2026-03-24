#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <arpa/inet.h>

/*
    TODO:
        Create chunk struct
        Verify png file
        Verify CRC of chunk
        Get metadata
        Add own chunk anywhere acceptable
        Modify png data
*/

const uint8_t header[] = {137, 80, 78, 71, 13, 10, 26, 10};
const uint8_t ihdr_ind[] = {'I', 'H', 'D', 'R'};

uint32_t crc_table[256];

void init_crc32() {
    uint32_t crc32 = 1;
    for (unsigned int i = 128; i; i >>= 1) {
        crc32 = (crc32 >> 1) ^ (crc32 & 1 ? 0xedb88320 : 0);
        for (unsigned int j = 0; j < 256; j += 2*i) {
            crc_table[i+j] = crc32 ^ crc_table[j];
        }
    }
}

uint32_t get_chunk_crc32(uint8_t *data, size_t data_length) {
    uint32_t crc32 = 0xFFFFFFFFu;
    if (crc_table[255] == 0) {
        init_crc32();
    }
    for (size_t i = 0; i < data_length; i++) {
        crc32 ^= data[i];
        crc32 = (crc32 >> 8) ^ crc_table[crc32 & 0xFF];
    }
    crc32 ^= 0xFFFFFFFFu;
    return crc32;
}

void print_each_byte(uint8_t *arr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%d ", arr[i]);
    }
    puts("");
}

void verify_png_file(FILE *file) {
    uint8_t *header_chunk = malloc(8 * sizeof(uint8_t));

    assert(fread(header_chunk, sizeof(uint8_t), 8, file) != 0);
    if (memcmp(header, header_chunk, 8) != 0) {
        fprintf(stderr, "PNG Header not found\n");
        exit(1);
    }

    printf("Header: ");
    print_each_byte(header_chunk, 8);

    uint32_t ihdr_len;
    assert(fread(&ihdr_len, sizeof(uint32_t), 1, file) != 0);
    
    uint8_t *ihdr_type = malloc(4 * sizeof(uint8_t));
    assert(fread(ihdr_type, sizeof(uint8_t), 4, file) != 0);
    if (memcmp(ihdr_ind, ihdr_type, 4) != 0) {
        fprintf(stderr, "IHDR not found\n");
        exit(1);
    }

    printf("IHDR: ");
    print_each_byte(ihdr_type, 4);

    free(header_chunk);
    free(ihdr_type);
}



int main(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "File not attached\n");
        exit(1);
    }

    FILE *png_file = fopen(argv[1], "rb");

    verify_png_file(png_file);

    fclose(png_file);


    return 0;
}