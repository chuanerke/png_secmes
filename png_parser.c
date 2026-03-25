#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <arpa/inet.h>

const uint8_t header[] = {137, 80, 78, 71, 13, 10, 26, 10};

const uint8_t ihdr_ind[] = {'I', 'H', 'D', 'R'};
const uint8_t iend_ind[] = {'I', 'E', 'N', 'D'};

struct Chunk {
    uint32_t length;
    uint8_t type[4];
    uint8_t *data;
    uint32_t crc;  
};

struct Metadata {
    uint32_t height;
    uint32_t width;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t comp_method;
    uint8_t filt_method;
    uint8_t int_method;
};

struct File_chunks {
    struct Chunk *chunks;
    size_t count;
    size_t capacity;
};

#define fc_append(xs, x)\
    do {\
        if (xs->count >= xs->capacity) {\
            if (xs->capacity == 0) xs->capacity = 3; else xs->capacity *= 2;\
            xs->chunks = realloc(xs->chunks, xs->capacity*(sizeof(struct Chunk)));\
        }\
        xs->chunks[xs->count++] = x;\
    } while(0)

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
    
    printf("CRC: %d\n", htonl(p_chunk.crc));
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

void add_file_chunks(FILE *file, struct File_chunks *chunks) {
    while (1) {
        struct Chunk temp;
        parse_chunk(file, &temp);
        fc_append(chunks, temp);
        if (memcmp(iend_ind, temp.type, 4) == 0) return;
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
    
    uint32_t crc;
    assert(fread(&crc, sizeof(uint32_t), 1, file) != 0);

    printf("IHDR CRC: %d\n", crc);

    free(header_chunk);
    free(ihdr_type);
    free(ihdr_data);
}

void get_metadata_from_ihdr(struct Chunk ihdr, struct Metadata *metadata) {
    metadata->height = ihdr.data[0] | (ihdr.data[1] << 8) | 
                    (ihdr.data[2] << 16) | (ihdr.data[3] << 24);
    metadata->width = ihdr.data[4] | (ihdr.data[5] << 8) | 
                    (ihdr.data[6] << 16) | (ihdr.data[7] << 24);

    metadata->height = ntohl(metadata->height);
    metadata->width = ntohl(metadata->width);

    metadata->bit_depth = ihdr.data[8];
    metadata->color_type = ihdr.data[9];

    metadata->comp_method = ihdr.data[10];
    metadata->filt_method = ihdr.data[11];
    metadata->int_method = ihdr.data[12];
}

void print_metadata(struct Metadata metadata) {
    printf("File metadata:\n");

    printf("Resolution: %d x %d\n", metadata.height, metadata.width);
    printf("Bit depth: %d\n", metadata.bit_depth);
    printf("Color type: %d\n", metadata.color_type);
    printf("Compression method: ");
    !(metadata.comp_method) ? printf("Deflate/Inflate Compression\n") : printf("Unknown\n");

    printf("Filter method: ");
    !(metadata.filt_method) ? printf("Adaptive filtering\n") : printf("Unknown\n");

    printf("Interlace method: ");
    !(metadata.int_method) ? printf("No interlace\n") : printf("Adam7 interlace\n");
}

int main(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "File not attached\n");
        exit(1);
    }

    FILE *png_file = fopen(argv[1], "rb");

    struct File_chunks file_data;

    print_and_verify_header(png_file);
    add_file_chunks(png_file, &file_data);

    struct Metadata metadata;

    uint8_t temp_type[4];
    for (size_t count = 0; memcmp(iend_ind, temp_type, 4) != 0; count++) {
        memcpy(temp_type, file_data.chunks[count].type, 4);
        print_chunk(file_data.chunks[count]);
        if (memcmp(ihdr_ind, temp_type, 4) == 0) {
            get_metadata_from_ihdr(file_data.chunks[count], &metadata);
            print_metadata(metadata);
        }
    }
    fclose(png_file);
    return 0;
}