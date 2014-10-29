#ifndef BTC_H
#define BTC_H

#define BUF_SIZE        256
#define BLOCK_SIZE      8
#define BITS_IN_BYTE    8

//macro for easily getting how much time has passed between two events
#define GET_TIME_DELTA(t1, t2) ((t2).tv_sec - (t1).tv_sec + \
        ((t2).tv_usec - (t1).tv_usec) / 1000000.0)


// 8 * 8 pixel block
struct pixel_block
{
    short int pixels[BLOCK_SIZE * BLOCK_SIZE] __attribute__ ((aligned(16)));
} __attribute__ ((aligned(16)));


// We store the imagine in blocks of pixels, not as a height * width array
// in order to simplify DMA transfers and general processing
struct img
{
    //regular image
    int width, height;
    struct pixel_block* pixel_blocks;
} __attribute__ ((aligned(16)));

struct block
{
    //data for a block from the compressed image
    unsigned char a, b;
    short int bitplane[BLOCK_SIZE * BLOCK_SIZE] __attribute__ ((aligned(16)));
    //one byte for each bit in the bitplane
    //quite memory inefficient, but let's keep it simple
} __attribute__ ((aligned(16)));

struct c_img
{
    //compressed image
    int width, height;
    struct block* blocks;
} __attribute__ ((aligned(16)));

typedef struct Info
{
    struct img image;
    struct img image2;
    struct c_img c_image;
    
    unsigned int nr_img_blocks;
    unsigned int nr_blocks_per_transfer;
    unsigned int mode;
}  __attribute__ ((aligned(16))) Info;

struct bits
{
    unsigned bit0 : 1;
    unsigned bit1 : 1;
    unsigned bit2 : 1;
    unsigned bit3 : 1;
    unsigned bit4 : 1;
    unsigned bit5 : 1;
    unsigned bit6 : 1;
    unsigned bit7 : 1;
};


typedef enum Phase { NONE, COMPRESS, DECOMPRESS, FINISHED } Phase;
void next_phase(Phase* phase);

#define FINISHED_PHASE 1
#define FINISHED_BLOCK_RANGE 2


//utils
void* _alloc(int size);
void _read_buffer(int fd, void* buf, int size);
void _write_buffer(int fd, void* buf, int size);
int _open_for_write(char* path);
int _open_for_read(char* path);
//read_btc
void read_btc(char* path, struct c_img* out_img);
void write_btc(char* path, struct c_img* out_img);
void free_btc(struct c_img* out_img);
//read_pgm
void read_pgm(char* path, struct img* in_img);
void write_pgm(char* path, struct img* out_img);
void free_pgm(struct img* out_img);

#endif
