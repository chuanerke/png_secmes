#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <arpa/inet.h>

const uint8_t header[] = {137, 80, 78, 71, 13, 10, 26, 10};
const uint8_t iend_ind[] = {73, 69, 78, 68};

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

void dump_png_file(FILE *file) {
    FILE *png_contents = fopen("png_dump.txt", "w");
    uint8_t *byte_section = malloc(4 * sizeof(uint8_t)); 
    while (fread(byte_section, sizeof(uint8_t), 4, file) != 0) {
        for (size_t i = 0; i < 4; i++) {
            fprintf(png_contents, "%d ", byte_section[i]);
        }
        fprintf("\n");
    }
    rewind(file);
    fclose(png_contents);
    free(byte_section);
}


void write_png_file_till_IEND(FILE *file, FILE *w_file) {
    uint8_t *first_8_bytes = malloc(8 * sizeof(uint8_t));

    assert(fread(first_8_bytes, sizeof(uint8_t), 8, file) != 0);
    if (memcmp(header, first_8_bytes, 8) != 0) {
        fprintf(stderr, "PNG header not found\n");
        exit(1);
    }

    fwrite(first_8_bytes, sizeof(uint8_t), 8, w_file);

    printf("Header: ");
    for ( ; *first_8_bytes != 0; first_8_bytes++) {
        printf("%d ", *first_8_bytes);
    }
    puts("");

    uint8_t *file_ptr = malloc(1 * sizeof(uint8_t));
    uint8_t *file_iend = malloc(4 * sizeof(uint8_t));
    size_t fcount = 0;
    while (fread(file_ptr, sizeof(uint8_t), 1, file) != 0) {
        fwrite(file_ptr, sizeof(uint8_t), 1, w_file);
        if (*file_ptr == iend_ind[fcount]) {
            file_iend[fcount++] = *file_ptr;
            if (memcmp(iend_ind, file_iend, 4) == 0) {
                // without %.4s there's corrupted data in stdout 
                // since there's no '\0'
                printf("Chunk: %.4s\n", file_iend);
                if (fseek(file, -8, SEEK_CUR) == -1 || fseek(w_file, -8, SEEK_CUR) == -1) {
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

void write_chunk_to_file(FILE *file, FILE *w_file, char type_name[4], 
                        char *data, size_t data_length) {
    write_png_file_till_IEND(file, w_file);
    uint32_t nt_data_length = htonl(data_length);
    fwrite(&nt_data_length, 4, 1, w_file);
    size_t pre_bytes_size = sizeof(char) * (data_length + 4);
    char *pre_bytes_buf = malloc(pre_bytes_size);
    char *pre_bytes_ptr = (char *) mempcpy(
                                mempcpy((void *)pre_bytes_buf, (void *)type_name, 4), 
                                (void *)data, data_length);
    fwrite(pre_bytes_buf, 1, (data_length + 4), w_file);
    uint32_t crc = get_chunk_crc32((uint8_t *)pre_bytes_buf, pre_bytes_size);
    crc = htonl(crc);
    fwrite(&crc, 4, 1, w_file);

    uint8_t file_ptr;
    while (fread(&file_ptr, sizeof(uint8_t), 1, file) != 0) {
        fwrite(&file_ptr, sizeof(uint8_t), 1, w_file);
    }

    free(pre_bytes_buf);
    (void) pre_bytes_ptr;
}


int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Both files not attached\n");
        exit(1);
    }

    FILE *png_file = fopen(argv[1], "rb");
    FILE *w_file = fopen("dest.png", "wb");

    char type_name[4] = {'m', 'i', 'N', 'e'};
    char data[] = "Whateverdatak";

    write_chunk_to_file(png_file, w_file, type_name, data, strlen(data));

    fclose(png_file);
    fclose(w_file);
    return 0;
}