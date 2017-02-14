/* Generate a random maze and output to BMP file */
/* NOTE: multi-byte values have to be written in little-endian order, as per the BMP standard */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

/* directions */
enum {
    NORTH,
    EAST,
    SOUTH,
    WEST
};

/* Maze dimensions and masks */
#define MAZE_W           (256)
#define MAZE_H           (256)
#define MAZE_A           (MAZE_W * MAZE_H)
#define TILE_N           (1 << 0)
#define TILE_E           (1 << 1)
#define TILE_S           (1 << 2)
#define TILE_W           (1 << 3)
#define TILE_UNVISITED   (0)

/* BMP region sizes and offsets */
#define FILE_H_SZ        (14)
#define INFO_H_SZ        (12)
#define COLOR_TBL_SZ     ((1 << PIX_BIT) * 3)
#define IMG_OFFSET       (FILE_H_SZ + INFO_H_SZ + COLOR_TBL_SZ)

/* BMP byte (octet) and bit dimensions */
#define PIX_BIT          (1)
#define TILE_PIX_W       (1)
#define TILE_PIX_H       (TILE_PIX_W)
#define IMG_PIX_W        ((MAZE_W * 2 + 1) * TILE_PIX_W)
#define IMG_PIX_H        ((MAZE_H * 2 + 1) * TILE_PIX_H)
#define IMG_PACKED_ROW_W ((PIX_BIT * IMG_PIX_W + 31) / 8)
#define IMG_BYTES        (IMG_PACKED_ROW_W * IMG_PIX_H)
#define FILE_SZ          (IMG_OFFSET + IMG_BYTES)

/* BMP data and maze buffers */
static uint8_t mazebuf[MAZE_A], filebuf[FILE_SZ];
static uint8_t *img_data = &filebuf[IMG_OFFSET + IMG_PACKED_ROW_W];

/* Functions to convert multi-byte values to LE (required by BMP) */
static inline int is_be(void) {
    uint16_t i = 1U;
    return(((uint8_t*)&i)[0] == 0);
}
static uint32_t le_32(uint32_t i) {
    uint8_t t, *p;
    if(is_be()) {
        p = (uint8_t*)&i;
        t = p[0];
        p[0] = p[3];
        p[3] = t;
        t = p[1];
        p[1] = p[2];
        p[2] = t;
    }
    return(i);
}
static uint16_t le_16(uint16_t i) {
    uint8_t t, *p;
    if(is_be() ) {
        p = (uint8_t*)&i;
        t = p[0];
        p[0] = p[1];
        p[1] = t;
    }
    return(i);
}

/* Write BMP regions to file */
/* NOTE: Reserved bytes 6-9 are not modified */
static void write_file_header(void) {
    uint32_t *p32;
    filebuf[0] = 'B';
    filebuf[1] = 'M';
    p32 = (uint32_t*)(&filebuf[2]);
    *p32 = le_32(FILE_SZ);
    p32 = (uint32_t*)(&filebuf[10]);
    *p32 = le_32(IMG_OFFSET);
}

static void write_info_header(void) {
    uint16_t* p16;
    uint32_t* p32;
    p32 = (uint32_t*)(&filebuf[14]);
    *p32 = le_32(INFO_H_SZ);
    p16 = (uint16_t*)(&filebuf[18]);
    *p16 = le_16(IMG_PIX_W);
    p16 = (uint16_t*)(&filebuf[20]);
    *p16 = le_16(IMG_PIX_H);
    p16 = (uint16_t*)(&filebuf[22]);
    *p16 = le_16(1);
    p16 = (uint16_t*)(&filebuf[24]);
    *p16 = le_16(PIX_BIT);
}
static void write_color_table(void) {

    /* color 0 (RGB) */
    filebuf[26] = 0x00U;
    filebuf[27] = 0x00U;
    filebuf[28] = 0x00U;
    
    /* color 1 (RGB) */
    filebuf[29] = 0xFFU;
    filebuf[30] = 0xFFU;
    filebuf[31] = 0xFFU;
}

/* Generate the maze using randomized recursive DFS */
static void gen_maze_dfs(int current) {
    int i, j, t, next = -1, row_off, col_off, req_bit, req_byte;
    
    /* visit directions (will be randomly shuffled with each call) */
    uint8_t dirs[4] = {NORTH, EAST, SOUTH, WEST};
    
    for(i = 3; i > 0; --i) {
        j = rand() % (i + 1);
        t = dirs[i];
        dirs[i] = dirs[j];
        dirs[j] = t;
    }
    
    /* locate & flip bmp pixel for visited tile */
    row_off  = current / MAZE_W;
    col_off  = current % MAZE_W;
    req_bit  = (row_off * IMG_PACKED_ROW_W * 8 * 2) + (col_off * 2) + 1;
    req_byte = req_bit / 8;
    req_bit  = req_bit % 8;
    img_data[req_byte] |= (0x80U >> req_bit);
    
    /* inspect all neighbours randomly */
    for(i = 0; i < 4; ++i) {
        switch(dirs[i]) {
            
        case NORTH:
            if(current >= MAZE_W && mazebuf[(next = current - MAZE_W)] == TILE_UNVISITED) {
                mazebuf[current] |= TILE_N;
                mazebuf[next] |= TILE_S;
                img_data[req_byte - IMG_PACKED_ROW_W] |= (0x80U >> req_bit);
                gen_maze_dfs(next);
            }
            break;
            
        case EAST:
            if(current % MAZE_W != (MAZE_W - 1) && (mazebuf[(next = current + 1)] == TILE_UNVISITED) ) {
                mazebuf[current] |= TILE_E;
                mazebuf[next] |= TILE_W;
                if(req_bit == 7) {
                    img_data[req_byte + 1] |= 0x80U;
                } else {
                    img_data[req_byte] |= (0x80U >> ((req_bit % 8) + 1));
                }
                gen_maze_dfs(next);
            }
            break;
            
        case SOUTH:
            if( (current < (MAZE_A - MAZE_W) ) && (mazebuf[(next = current + MAZE_W)] == TILE_UNVISITED) ) {
                mazebuf[current] |= TILE_S;
                mazebuf[next] |= TILE_N;
                img_data[req_byte + IMG_PACKED_ROW_W] |= (0x80U >> req_bit);
                gen_maze_dfs(next);
            }
            break;
            
        case WEST:
            if(col_off && (mazebuf[(next = current - 1)] == TILE_UNVISITED) ) {
                mazebuf[current] |= TILE_W;
                mazebuf[next] |= TILE_E;
                if(req_bit) {
                    img_data[req_byte] |= (0x80U >> ((req_bit % 8) - 1));
                } else {
                    img_data[req_byte - 1] |= (0x80U >> 7);
                }
                gen_maze_dfs(next);
            }
            break;
        }
    }
}

/* MAIN */
int main(void) {
    FILE *of;
    
    /* init */
    srand(time(NULL));
    
    /* Write BMP headers */
    write_file_header();
    write_info_header();
    write_color_table();
    
    /* Generate maze and associated pixel data */
    gen_maze_dfs(rand() % MAZE_A);
    
    /* Output BMP file */ 
    if((of = fopen("./maze.bmp", "wb"))) {
        fwrite(filebuf, sizeof(uint8_t), FILE_SZ, of);
        fclose(of);
    } else {
        printf("ERROR: Failed to write output file.\n");
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
