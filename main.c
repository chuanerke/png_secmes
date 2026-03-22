#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <arpa/inet.h>

/*
    Verify that it is a png file (header)
    Create private ancillary chunk
        Create a function to create chunk with data needed to be inserted which would require
        a CRC algorithm function for this 
        Also create function for the chunk type to fit requirements for each 5 bit
    Search file until IEND chunk
    Write our chunk before IEND (NOTE: This is easily found out, could change later)
*/

const uint8_t header[] = {137, 80, 78, 71, 13, 10, 26, 10};
const uint8_t iend_ind[] = {73, 69, 78, 68};

uint32_t crc_table[256];


void dump_png_file(FILE *file) {
    FILE *png_contents = fopen("png_dump.txt", "w");
    uint8_t *byte_section = malloc(4 * sizeof(uint8_t)); 
    while (fread(byte_section, sizeof(uint8_t), 4, file) != 0) {
        
        for (size_t i = 0; i < 4; i++) {
            fprintf(png_contents, "%d ", byte_section[i]);
        }
        fprintf(png_contents, "| %s\n", byte_section);
    }
    rewind(file);
    fclose(png_contents);
    free(byte_section);
}




void read_png_file_till_IEND(FILE *file) {
    uint8_t *first_8_bytes = malloc(8 * sizeof(uint8_t));
    
    assert(fread(first_8_bytes, sizeof(uint8_t), 8, file) != 0);
    if (memcmp(header, first_8_bytes, 8) != 0) {
        fprintf(stderr, "PNG header not found\n");
        exit(1);
    }
    printf("Header: ");
    for ( ; *first_8_bytes != 0; first_8_bytes++) {
        printf("%d ", *first_8_bytes);
    }
    puts("");

    uint8_t *file_ptr = malloc(1 * sizeof(uint8_t));


    // uint32_t *f_iend_length = malloc(1 * sizeof(uint32_t));
    // uint32_t f_iend_length;
    uint8_t *file_iend = malloc(4 * sizeof(uint8_t));
    size_t fcount = 0;
    while (fread(file_ptr, sizeof(uint8_t), 1, file) != 0) {
        if (*file_ptr == iend_ind[fcount]) {
            file_iend[fcount++] = *file_ptr;
            if (memcmp(iend_ind, file_iend, 4) == 0) {
                // without %.4s there's corrupted data in stdout 
                // since there's no '\0'
                printf("Chunk: %.4s\n", file_iend);
                if (fseek(file, -8, SEEK_CUR) == -1) {
                    perror("fseek");
                    exit(1);
                }
                break;
            }
        } else {
            fcount = 0;
        }
    }

    // while (1) {
    //     fread(&f_iend_length, sizeof(uint32_t), 1, file);
    //     f_iend_length = ntohl(f_iend_length);
    //     fread(f_iend, sizeof(uint8_t), 4, file);
    //     if (memcmp(iend_ind, f_iend, 4) == 0) {
    //         printf("Chunk: %.4s\n", f_iend);
    //         if (fseek(file, -f_iend_length, SEEK_CUR) == -1) {
    //             perror("fseek");
    //             exit(1);
    //         }
    //         break;
    //     }
    // }
    if (!fcount) {
        fprintf(stderr, "IEND chunk not found\n");
        exit(1);
    }
    free(file_ptr);
}



void init_crc32() {
    uint32_t crc32 = 1;

    for (uint8_t i = 128; ; i >>= 1) {
        crc32 = (crc32 >> 1) ^ (crc32 & 1 ? 0xedb88320 : 0);
        for (uint8_t j = 0; j < 256; j += 2*i) {
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



void write_chunk_to_file(FILE* file, uint8_t type_name[4], 
                        char *data, size_t data_length) {
    read_png_file_till_IEND(file);
    fwrite(&data_length, 4, 1, file);
    fwrite(&type_name, 1, 4, file);
    fwrite(&data, 1, data_length, file);
    uint8_t *pre_bytes = malloc(sizeof(uint8_t) * (data_length + 4) + 1);
    pre_bytes = (uint8_t *) strcat(type_name, data);
    uint32_t crc = get_chunk_crc32(pre_bytes, sizeof(pre_bytes));
    fwrite(&crc, 4, 1, file);
    
    free(pre_bytes);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "No file attached\n");
        exit(1);
    }

    FILE *png_file;
    png_file = fopen(argv[1], "rb+");

    // dump_png_file(png_file);

    // FILE *file_wr;
    // file_wr = fopen(argv[1], "ab+");
    // while (fwrite()) {

    // }
    uint8_t type_name[4] = "miNe";
    char data[] = "Whatever data";
    write_chunk_to_file(png_file, type_name, data, 4);

    fclose(png_file);
    return 0;
}