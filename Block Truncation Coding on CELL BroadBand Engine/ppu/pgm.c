#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libmisc.h> 

#include "btc.h"

void read_line(int fd, char* path, char* buf, int buf_size){
    char c = 0;
    int i = 0;
    while (c != '\n'){
        if (read(fd, &c, 1) == 0){
            fprintf(stderr, "Error reading from %s\n", path);
            exit(0);
        }
        if (i == buf_size){
            fprintf(stderr, "Unexpected input in %s\n", path);
            exit(0);
        }
        buf[i++] = c;
    }
    buf[i] = '\0';
}

void read_pgm(char* path, struct img* in_img){
    int fd;
    int image_blocks;
    int image_width_blocks;
    int image_height_blocks;
    char buf[BUF_SIZE], *token;
    unsigned char* tmp_pixels;
    
    register int i, j, k;
    register struct pixel_block* block;
    register unsigned char* pixel_ptr;
    register int row_start;

    fd = _open_for_read(path);

    //read file type; expecting P5
    read_line(fd, path, buf, BUF_SIZE);
    if (strncmp(buf, "P5", 2)){
        fprintf(stderr, "Expected binary PGM (P5 type), got %s\n", path);
        exit(0);
    }

    //read comment line
    read_line(fd, path, buf, BUF_SIZE);

    //read image width and height
    read_line(fd, path, buf, BUF_SIZE);
    token = strtok(buf, " ");
    if (token == NULL){
        fprintf(stderr, "Expected token when reading from %s\n", path);
        exit(0);
    }   
    in_img->width = atoi(token);
    token = strtok(NULL, " ");
    if (token == NULL){
        fprintf(stderr, "Expected token when reading from %s\n", path);
        exit(0);
    }
    in_img->height = atoi(token);
    if (in_img->width < 0 || in_img->height < 0){
        fprintf(stderr, "Invalid width or height when reading from %s\n", path);
        exit(0);
    }

    //read max value
    read_line(fd, path, buf, BUF_SIZE);
    
    image_height_blocks = (in_img->height / BLOCK_SIZE);
    image_width_blocks = (in_img->width / BLOCK_SIZE);
    image_blocks = (in_img->width * in_img->height) / (BLOCK_SIZE * BLOCK_SIZE);

    //allocate memory for image pixels
    tmp_pixels = _alloc(in_img->width * in_img->height * sizeof(unsigned char));    
    in_img->pixel_blocks = _alloc(image_blocks * sizeof (struct pixel_block));

    _read_buffer(fd, tmp_pixels, in_img->width * in_img->height);
    
    pixel_ptr = tmp_pixels;
    block = in_img->pixel_blocks;
    for(i = 0; i < image_height_blocks; i++)
    {
        for(k = 0; k < BLOCK_SIZE; k++)
        {
            row_start = k * BLOCK_SIZE;
            for(j = 0; j < image_width_blocks; j++)
            {
                block->pixels[row_start + 0] = (short int)pixel_ptr[0];
                block->pixels[row_start + 1] = (short int)pixel_ptr[1];
                block->pixels[row_start + 2] = (short int)pixel_ptr[2];
                block->pixels[row_start + 3] = (short int)pixel_ptr[3];
                block->pixels[row_start + 4] = (short int)pixel_ptr[4];
                block->pixels[row_start + 5] = (short int)pixel_ptr[5];
                block->pixels[row_start + 6] = (short int)pixel_ptr[6];
                block->pixels[row_start + 7] = (short int)pixel_ptr[7];
                
                block++;
                pixel_ptr += BLOCK_SIZE;
            }
            block -= image_width_blocks;
        }
        block += image_width_blocks;
    }

    
    free_align(tmp_pixels);
    close(fd);
}

void write_pgm(char* path, struct img* out_img){
    int fd;
    int image_blocks;
    int image_height_blocks;
    int image_width_blocks;
    char buf[BUF_SIZE];
    unsigned char* tmp_pixels;
    
    register int i, j, k;
    register struct pixel_block* block;
    register unsigned char* pixel_ptr;
    register int row_start;

    fd = _open_for_write(path);

    //write image type
    strcpy(buf, "P5\n");
    _write_buffer(fd, buf, strlen(buf));

    //write comment 
    strcpy(buf, "#Created using BTC\n");
    _write_buffer(fd, buf, strlen(buf));

    //write image width and height
    sprintf(buf, "%d %d\n", out_img->width, out_img->height);
    _write_buffer(fd, buf, strlen(buf));

    //write max value
    strcpy(buf, "255\n");
    _write_buffer(fd, buf, strlen(buf));

    tmp_pixels = calloc(out_img->width * out_img->height, sizeof (char));
    if (!tmp_pixels){
        fprintf(stderr, "Error allocating memory when reading from %s\n", path);
        exit(0);
    }
    
    image_height_blocks = (out_img->height / BLOCK_SIZE);
    image_width_blocks = (out_img->width / BLOCK_SIZE);
    image_blocks = (out_img->width * out_img->height) / (BLOCK_SIZE * BLOCK_SIZE);
    
    pixel_ptr = tmp_pixels;
    block = out_img->pixel_blocks;
    for(i = 0; i < image_height_blocks; i++)
    {
        for(k = 0; k < BLOCK_SIZE; k++)
        {
            row_start = k * BLOCK_SIZE;
            for(j = 0; j < image_width_blocks; j++)
            {
                pixel_ptr[0] = (unsigned char)block->pixels[row_start + 0];
                pixel_ptr[1] = (unsigned char)block->pixels[row_start + 1];
                pixel_ptr[2] = (unsigned char)block->pixels[row_start + 2];
                pixel_ptr[3] = (unsigned char)block->pixels[row_start + 3];
                pixel_ptr[4] = (unsigned char)block->pixels[row_start + 4];
                pixel_ptr[5] = (unsigned char)block->pixels[row_start + 5];
                pixel_ptr[6] = (unsigned char)block->pixels[row_start + 6];
                pixel_ptr[7] = (unsigned char)block->pixels[row_start + 7];
                
                block++;
                pixel_ptr += BLOCK_SIZE;
            }
            block -= image_width_blocks;
        }
        block += image_width_blocks;
    }

    //write image pixels
    _write_buffer(fd, tmp_pixels, out_img->width * out_img->height);

    free(tmp_pixels);
    close(fd);
}

void free_pgm(struct img* image){
    free_align(image->pixel_blocks);
}
