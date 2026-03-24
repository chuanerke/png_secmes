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

/* 
    Check & Parse chunks
    Error-Check for valid png file:
    1. Header -> IHDR -> IDAT -> IEND
    2. IHDR -> PLTT
    3. IDAT -> IDAT
    4. Verify CRC
    
    Read IHDR, print metadata
    Check and read any ancillary chunks
*/

// #define #x x 
const uint8_t header[] = {137, 80, 78, 71, 13, 10, 26, 10};

// Critical Chunks
const uint8_t ihdr_ind[] = {'I', 'H', 'D', 'R'};

struct Chunk {
    uint32_t length; // needs to be after htonl (might change) 
    uint8_t type[4];
    uint8_t *data;
    uint32_t crc;  
};

void print_each_byte(uint8_t *arr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%d ", arr[i]);
    }
    puts("");
}

void print_and_verify_header(FILE *file) {
    uint8_t *header_chunk = malloc(8 * sizeof(uint8_t));

    assert(fread(header_chunk, sizeof(uint8_t), 8, file) != 0);
    if (memcmp(header, header_chunk, 8) != 0) {
        fprintf(stderr, "PNG Header not found\n");
        exit(1);
    }

    printf("Header: ");
    print_each_byte(header_chunk, 8);

    free(header_chunk);
}

void print_chunk(struct Chunk p_chunk) {
    printf("Chunk: %.4s\n", p_chunk.type);
    
    uint32_t nt_length = htonl(p_chunk.length);
    printf("Length: %d\n", nt_length);
    
    printf("Data: \n");
    print_each_byte(p_chunk.data, nt_length);

    printf("CRC: %d\n", p_chunk.crc);
}


void parse_chunk(FILE *file, struct Chunk *curr_chunk) {
    size_t fr_ret;

    fr_ret = fread(&curr_chunk->length, sizeof(uint32_t), 1, file);
    if (fr_ret != 1) {
        fprintf(stderr, "Chunk length could not be read\n");
        exit(1);
    }

    fr_ret = fread(&curr_chunk->type, sizeof(uint8_t), 4, file);
    if (fr_ret != 4) {
        fprintf(stderr, "Chunk type could not be read\n");
        exit(1);
    }
    
    uint32_t nt_length = htonl(curr_chunk->length);
    curr_chunk->data = malloc(sizeof(uint8_t) * nt_length);
    fr_ret = fread(curr_chunk->data, sizeof(uint8_t), nt_length, file);
    if (fr_ret != nt_length) {
        fprintf(stderr, "Chunk length could not be read\n");
        exit(1);
    }
    
    fr_ret = fread(&curr_chunk->crc, sizeof(uint32_t), 1, file);
    if (fr_ret != 1) {
        fprintf(stderr, "Chunk CRC could not be read\n");
        exit(1);
    }
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

    ihdr_len = htonl(ihdr_len);

    uint8_t *ihdr_data = malloc(ihdr_len * sizeof(uint8_t));
    assert(fread(ihdr_data, sizeof(uint8_t), ihdr_len, file) != 0);

    printf("IHDR Length: %d\n", ihdr_len);
    printf("IHDR: ");
    print_each_byte(ihdr_type, 4);
    printf("IHDR data:\n");
    print_each_byte(ihdr_data, ihdr_len);
    // printf("%c\n", ihdr_data);

    uint32_t crc;
    assert(fread(&crc, sizeof(uint32_t), 1, file) != 0);

    printf("IHDR CRC: %d\n", crc);

    free(header_chunk);
    free(ihdr_type);
    free(ihdr_data);
}


int main(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "File not attached\n");
        exit(1);
    }

    FILE *png_file = fopen(argv[1], "rb");

    // verify_png_file(png_file);
    print_and_verify_header(png_file);
    struct Chunk ihdr;
    parse_chunk(png_file, &ihdr);
    print_chunk(ihdr);

    struct Chunk idat;
    parse_chunk(png_file, &idat);
    print_chunk(idat);

    struct Chunk iend;
    parse_chunk(png_file, &iend);
    print_chunk(iend);
    

    fclose(png_file);
    return 0;
}