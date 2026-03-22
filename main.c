#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

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


void dump_png_file(FILE *file) {
    FILE *png_contents = fopen("png_dump.txt", "w");
    uint8_t *byte_section = malloc(4 * sizeof(uint8_t)); 
    while (fread(byte_section, sizeof(uint8_t), 4, file) != NULL) {
        
        for (size_t i = 0; i < 4; i++) {
            fprintf(png_contents, "%d ", byte_section[i]);
        }
        //fputs("\n", png_contents);

        fprintf(png_contents, "| %s\n", byte_section);
    }
    rewind(file);
    fclose(png_contents);
    free(byte_section);
}


void read_png_file_till_IEND(FILE *file) {
    // if (ftell(file) != SEEK_SET) {
    //     fseek(file, 0L, SEEK_SET);
    // }
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
    if (!fcount) {
        fprintf(stderr, "IEND chunk not found\n");
        exit(1);
    }
    free(file_ptr);
}

void convert_to_network_order() {
    // htons should suffice?
}

uint32_t get_chunk_crc32(uint8_t *type, 
                        uint8_t *data, size_t data_length) {
    // https://en.wikipedia.org/wiki/Computation_of_cyclic_redundancy_checks#CRC-32_example
}

void write_chunk_to_file(FILE* file, char *type_name, 
                        char *data, size_t data_length) {
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "No file attached\n");
        exit(1);
    }

    FILE *png_file;
    png_file = fopen(argv[1], "rb");

    dump_png_file(png_file);

    read_png_file_till_IEND(png_file);

    fclose(png_file);
    return 0;
}