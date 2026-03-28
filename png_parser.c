#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <argp.h>

#include <arpa/inet.h>

/* TODO: LSB Stegnography */

const uint8_t header[] = {137, 80, 78, 71, 13, 10, 26, 10};

const uint8_t ihdr_ind[] = {'I', 'H', 'D', 'R'};
const uint8_t iend_ind[] = {'I', 'E', 'N', 'D'};

uint32_t crc_table[256];

static char doc[] = "PNG parser and data hiding tool.";
static char args_doc[] = "PNG PARSER";

static struct argp_option options[] = {
    {"print", 'p', NULL, 0, "print data (default:\"\")"},
    {"chunk", 'c', "CHUNK", 0, "chunk to print (default: \"\")"},
    {"hide", 'h', NULL, 0, "hide data in file (default:\"\")"},
    {"data", 'd', "DATA", 0, "the data to hide (default:\"\")"},
    {"outfile", 'o', "OUTFILE", 0, "file to output to (default: \"dest.png\")"},
    { 0 }
};


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

struct arguments {
    char *args[1];
    int print;
    char *chunk;
    int hide;
    char *data;
    char *outfile;
};

#define fc_append(xs, x)\
    do {\
        if (xs->count >= xs->capacity) {\
            if (xs->capacity == 0) xs->capacity = 3; else xs->capacity *= 2;\
            xs->chunks = realloc(xs->chunks, xs->capacity*(sizeof(struct Chunk)));\
        }\
        xs->chunks[xs->count++] = x;\
    } while(0)


static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    switch (key) {
        case 'p':
            arguments->print = 1;
            break;
        case 'c':
            arguments->chunk = arg;
            break;
        case 'h':
            arguments->hide = 1;
            break;
        case 'd':
            arguments->data = arg;
            break;
        case 'o':
            arguments->outfile = arg;
            break;
        
        case ARGP_KEY_ARG:
            if (state->arg_num >= 1) {
                argp_usage(state);
            }
            arguments->args[state->arg_num] = arg;
            break;
        
        case ARGP_KEY_END:
            if (state->arg_num < 1) {
                argp_usage(state);
            }
            break;
        
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

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
        fprintf(png_contents, "\n");
    }
    rewind(file);
    fclose(png_contents);
    free(byte_section);
}

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
    puts("");
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
    puts("");
}

void verify_file_chunks(struct File_chunks *file_data) {
    if (memcmp(ihdr_ind, file_data->chunks[0].type, 4)) {
        fprintf(stderr, "IHDR not found\n");
        exit(1);
    }

    if (memcmp(iend_ind, file_data->chunks[file_data->count - 1].type, 4)) {
        fprintf(stderr, "IEND not found\n");
        exit(1);
    } 
}

void print_file_chunks(struct File_chunks *file_data, struct Metadata *metadata) {
    verify_file_chunks(file_data);
    
    uint8_t temp_type[4];
    for (size_t count = 0; memcmp(iend_ind, temp_type, 4) != 0; count++) {
        memcpy(temp_type, file_data->chunks[count].type, 4);
        if (memcmp(ihdr_ind, temp_type, 4) == 0) {
            get_metadata_from_ihdr(file_data->chunks[count], metadata);

            print_metadata(*metadata);
        }
        print_chunk(file_data->chunks[count]);
    }
}

void write_chunk_to_file(struct Chunk chunk, FILE *w_file) {
    fwrite(&(chunk.length), 4, 1, w_file);
    fwrite(&(chunk.type), 1, 4, w_file);
    fwrite(chunk.data, 1, htonl(chunk.length), w_file);
    fwrite(&(chunk.crc), 4, 1, w_file);
}

void write_user_chunk_to_file(struct File_chunks *fchunks, FILE *w_file, char type_name[4],
                        char *data, size_t data_length) {
    uint8_t temp_type[4];
    for (size_t count = 0; memcmp(iend_ind, temp_type, 4) != 0; count++) {
        memcpy(temp_type, fchunks->chunks[count].type, 4);
        if (memcmp(iend_ind, temp_type, 4) != 0) {  
            write_chunk_to_file(fchunks->chunks[count], w_file);
        }
    }
    
    uint32_t nt_data_length = htonl(data_length);
    fwrite(&nt_data_length, 4, 1, w_file);
    size_t pre_bytes_size = sizeof(char) * (data_length + 4);
    char *pre_bytes_buf = malloc(pre_bytes_size);
    char *pre_bytes_ptr = (char *) mempcpy(
                                mempcpy(pre_bytes_buf, type_name, 4), 
                                data, data_length);
    fwrite(pre_bytes_buf, 1, (data_length + 4), w_file);
    uint32_t crc = get_chunk_crc32((uint8_t *)pre_bytes_buf, pre_bytes_size);
    crc = htonl(crc);
    fwrite(&crc, 4, 1, w_file);

    write_chunk_to_file(fchunks->chunks[--(fchunks->count)], w_file);

    free(pre_bytes_buf);
    (void) pre_bytes_ptr;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char **argv) {
    struct arguments arguments;

    arguments.print = 0;
    arguments.outfile = "dest.png";
    arguments.hide = 0;

    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    FILE *png_file = fopen(arguments.args[0], "rb");
    struct File_chunks *file_data = malloc(sizeof(struct File_chunks) * 3);
    file_data->capacity = 0;
    file_data->count = 0;

    print_and_verify_header(png_file);
    add_file_chunks(png_file, file_data);
    struct Metadata metadata;

    if (arguments.print == 1) {
        if (!arguments.chunk) {
            print_file_chunks(file_data, &metadata);
        } else {
            for (size_t i = 0; i < file_data->count; i++) {
                if (memcmp(arguments.chunk, file_data->chunks[i].type, 4) == 0) {
                    print_chunk(file_data->chunks[i]);
                }
            }
        }
    } 
    char user_type[4] = "usEr";
    if (arguments.hide == 1) {
        if (!arguments.data) {
            fprintf(stderr, "Need data to hide in file\n");
            exit(1);
        }
        FILE *out_file = fopen(arguments.outfile, "wb");
        
        write_user_chunk_to_file(file_data, out_file, user_type, 
                                arguments.data, strlen(arguments.data));
    }

    fclose(png_file);
    return 0;
}