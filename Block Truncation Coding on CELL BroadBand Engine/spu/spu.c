// Author: Catalin Constantin Usurelu
// Grupa: 333CA

#include <stdio.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include <libmisc.h>
#include <math.h>

#include "utils.h"
#include "../ppu/btc.h"

#include <time.h>
#include <sys/time.h>


#define waitag(t) mfc_write_tag_mask(1<<t); mfc_read_tag_status_all();

uint8_t spu_id;

float compute_block_mean(const struct pixel_block* pixel_block)
{
    float mean;
    int row;
    vector signed short v_sum = spu_splats((short int)0);
    vector signed short* v_pixel_block = (vector signed short*)pixel_block;
    
    for (row = 0; row < BLOCK_SIZE ; row++)
    {
        // Adun cate o linie vectorial
        v_sum += v_pixel_block[row];
    }
    
    mean = v_sum[0] + v_sum[1] +v_sum[2] + v_sum[3] + 
           v_sum[4] + v_sum[5] +v_sum[6] + v_sum[7];
    mean /= BLOCK_SIZE * BLOCK_SIZE;
    
    return mean;
}

float compute_block_stdev(const struct pixel_block* pixel_block, const float mean)
{
    float stdev = 0;
    int row;
    vector float v_stdev;
    vector float v_replicated_mean = spu_splats(mean);
    vector float v_pixels_float;
    vector signed short* v_pixel_block = (vector signed short*)pixel_block->pixels;

    for (row = 0; row < BLOCK_SIZE ; row++)
    {
        // Process each linie in 2 phases
        // In each phase we split the line in half (4 elements)
        // because we have to do vector operations on float.
        // A float vector has 4 elements while a short int vector has 8
        // so we need to convert the short int vector to float.
        v_pixels_float[0] = v_pixel_block[row][0];
        v_pixels_float[1] = v_pixel_block[row][1];
        v_pixels_float[2] = v_pixel_block[row][2];
        v_pixels_float[3] = v_pixel_block[row][3];
        
        v_stdev = v_pixels_float - v_replicated_mean;
        v_stdev = v_stdev * v_stdev;
        stdev += v_stdev[0] + v_stdev[1] + v_stdev[2] + v_stdev[3];
        
        v_pixels_float[0] = v_pixel_block[row][4];
        v_pixels_float[1] = v_pixel_block[row][5];
        v_pixels_float[2] = v_pixel_block[row][6];
        v_pixels_float[3] = v_pixel_block[row][7];
        
        v_stdev = v_pixels_float - v_replicated_mean;
        v_stdev = v_stdev * v_stdev;
        stdev += v_stdev[0] + v_stdev[1] + v_stdev[2] + v_stdev[3];
    }
    
    stdev /= BLOCK_SIZE * BLOCK_SIZE;
    stdev = sqrt(stdev);
    
    return stdev;
}

float compute_block_bitplane(const struct pixel_block* pixel_block,
                             short int* bitplane,
                             const float mean)
{
    float q = 0;
    int row;
    
    // Floor-ul din spu_splats((short int)mean) nu influenteaza neagtiv calculul
    // Exemplu : int > mean <=> int > floor(mean)
    vector signed short v_replicated_floored_mean = spu_splats((short int)mean);
    vector signed short* v_pixel_block = (vector signed short*)pixel_block;
    vector signed short * v_bitplane = (vector signed short*) bitplane;
    vector unsigned short v_tmp;
    vector unsigned int v_tmp2;
    vector unsigned char v_q;
    vector signed short ones = {1, 1, 1, 1, 1, 1, 1, 1};
    vector signed short zeroes = {0, 0, 0, 0, 0, 0, 0, 0};
    
    for (row = 0; row < BLOCK_SIZE; row++)
    {
        // spu_cmpgt(a, b) return a vector c where if a[i] > b[i]
        // c[i] is filled with 1's
        // else it is filled with 0's
        v_tmp = spu_cmpgt(v_pixel_block[row], v_replicated_floored_mean);

        // We only need the rezults from the comparison as 0 or 1 (not 111...)
        // so we extract the LSB's
        v_tmp2 = spu_gather(v_tmp);
        
        // Count the number of 1's
        v_q = spu_cntb((vector unsigned char)v_tmp2);
        q += v_q[0] + v_q[1] + v_q[2] + v_q[3];

        // Put the rezults of spu_cmpgt but using 1's and 0's, no 1111....
        v_bitplane[row] = spu_sel(zeroes, ones, v_tmp);
    }

    return q;
}

void compress(struct block* c_blocks, struct pixel_block* pixel_blocks, unsigned int transfered_blocks)
{
    unsigned int bl_index;
    float f1, f2, m, q, mean, stdev, a, b;
    
    m = BLOCK_SIZE * BLOCK_SIZE;
    
    // Compress blocks
    for(bl_index = 0; bl_index < transfered_blocks; bl_index++)
    {
        // Process 1 block from input image

        mean = compute_block_mean(pixel_blocks + bl_index);
        stdev = compute_block_stdev(pixel_blocks + bl_index, mean);
        q = compute_block_bitplane(pixel_blocks + bl_index, c_blocks[bl_index].bitplane, mean);
    
        // Compute a and b
        if (q == 0)
        {
            a = b = mean;
        }
        else
        {
            f1 = sqrt(q / (m - q));
            f2 = sqrt((m - q) / q);
            a = (mean - stdev * f1);
            b = (mean + stdev * f2);
        }
        
        //avoid conversion issues due to precision errors
        if (a < 0)
            a = 0;
        if (b > 255)
            b = 255;

        c_blocks[bl_index].a = (unsigned char)a;
        c_blocks[bl_index].b = (unsigned char)b;
    }
}

void compress_normal_dma(uint32_t tag_id, Info* info)
{
    unsigned int nr_blocks, nr_blocks_per_transfer, transfered_blocks;

    uint32_t block_range_start;
    uint32_t block_range_end;
    uint32_t task_block_range_start;
    uint32_t task_block_range_end;
    uint32_t msg;
    
    uint32_t size;
    uint32_t ea;
    
    nr_blocks =info->nr_img_blocks;
    nr_blocks_per_transfer = info->nr_blocks_per_transfer;

    // Allocate buffers
    struct block* c_blocks = _spu_alloc(nr_blocks_per_transfer * sizeof(struct block));
    struct pixel_block* pixel_blocks = _spu_alloc(nr_blocks_per_transfer * sizeof(struct pixel_block));
    
    // Receive task (block range to process)
    while (spu_stat_in_mbox()<=0);
    task_block_range_start = spu_read_in_mbox();
    
    while (spu_stat_in_mbox()<=0);
    task_block_range_end = spu_read_in_mbox();
    

    block_range_start = task_block_range_start;
    while(block_range_start < task_block_range_end)
    {
        block_range_end = block_range_start + nr_blocks_per_transfer < task_block_range_end ?
                          block_range_start + nr_blocks_per_transfer : task_block_range_end;
        transfered_blocks = block_range_end - block_range_start;
        
        size = transfered_blocks * sizeof(struct pixel_block);
        ea = (uint32_t)(info->image.pixel_blocks) + block_range_start * sizeof(struct pixel_block);
        mfc_get((void *)pixel_blocks, ea, size, tag_id, 0, 0);
        waitag(tag_id);
        
        // This is were the real work is done ...
        compress(c_blocks, pixel_blocks, transfered_blocks);
        
        // Transfer compressed blocks
        size = transfered_blocks * sizeof(struct block);
        ea = (uint32_t)(info->c_image.blocks) + block_range_start * sizeof(struct block);
        mfc_put((void *)c_blocks, ea, size, tag_id, 0, 0);
        waitag(tag_id);
        
        block_range_start += info->nr_blocks_per_transfer;
    }
    
    // Send reply - finished compression
    msg = FINISHED_PHASE;
    
    while((spu_stat_out_mbox() <= 0));
    spu_write_out_mbox(msg);
    
    free_align(c_blocks);
    free_align(pixel_blocks);
}

void decompress_block(const struct pixel_block* pixel_block,
                      short int * bitplane,
                      const unsigned char a,
                      const unsigned char b)
{
    int row;
    
    vector signed short v_replicated_a = spu_splats((signed short)a);
    vector signed short v_replicated_b = spu_splats((signed short)b);
    vector signed short* v_pixel_block = (vector signed short*)pixel_block;
    vector signed short * v_bitplane = (vector signed short*) bitplane;
    vector unsigned int v_tmp; 
    
    
    for (row = 0; row < BLOCK_SIZE; row++)
    {
        v_tmp = spu_gather(v_bitplane[row]);
        v_pixel_block[row] = spu_sel(v_replicated_a, v_replicated_b, spu_maskh(v_tmp[0]));
    }
}

void decompress(struct block* c_blocks, struct pixel_block* pixel_blocks, unsigned int transfered_blocks)
{
    unsigned int bl_index;
    unsigned char a, b;

    // Decompress blocks
    for(bl_index = 0; bl_index < transfered_blocks; bl_index++)
    {
        // Decompress one block at a time

        a = c_blocks[bl_index].a;
        b = c_blocks[bl_index].b;
        
        decompress_block(pixel_blocks + bl_index,
                         c_blocks[bl_index].bitplane,
                         a,
                         b);
    }
}


void decompress_normal_dma(uint32_t tag_id, Info* info)
{
    unsigned int nr_blocks_per_transfer, nr_blocks, transfered_blocks;
    
    uint32_t block_range_start;
    uint32_t block_range_end;
    uint32_t task_block_range_start;
    uint32_t task_block_range_end;
    uint32_t msg;
    
    uint32_t size;
    uint32_t ea;
    
    nr_blocks = info->nr_img_blocks;
    nr_blocks_per_transfer = info->nr_blocks_per_transfer;
    
    // Allocate buffers
    struct block* c_blocks = _spu_alloc(nr_blocks_per_transfer * sizeof(struct block));
    struct pixel_block* pixel_blocks = _spu_alloc(nr_blocks_per_transfer * sizeof(struct pixel_block));


    while (spu_stat_in_mbox()<=0);
    task_block_range_start = spu_read_in_mbox();
    
    while (spu_stat_in_mbox()<=0);
    task_block_range_end = spu_read_in_mbox();
    

    block_range_start = task_block_range_start;
    while(block_range_start < task_block_range_end)
    {
        block_range_end = block_range_start + nr_blocks_per_transfer < task_block_range_end ?
                          block_range_start + nr_blocks_per_transfer : task_block_range_end;
        transfered_blocks = block_range_end - block_range_start;
        
        // Get blocks to process
        size = transfered_blocks * sizeof(struct block);
        ea = (uint32_t)(info->c_image.blocks) + block_range_start * sizeof(struct block);
        mfc_get((void *)c_blocks, ea, size, tag_id, 0, 0);
        waitag(tag_id);
        
        // This is were the real work is done ...
        decompress(c_blocks, pixel_blocks, transfered_blocks);
        
        // Transfer decompressed pixels
        size = transfered_blocks * sizeof(struct pixel_block);
        ea = (uint32_t)(info->image2.pixel_blocks) + block_range_start * sizeof(struct pixel_block);
        mfc_put((void *)pixel_blocks, ea, size, tag_id, 0, 0);
        waitag(tag_id);

        block_range_start += info->nr_blocks_per_transfer;
    }
    
    // Send reply - finished decompression
    msg = FINISHED_PHASE;
    
    while((spu_stat_out_mbox() <= 0));
    spu_write_out_mbox(msg);
    
    free_align(c_blocks);
    free_align(pixel_blocks);
}

void compress_double_buffering_dma(uint32_t* tag_id, Info* info)
{
    unsigned int nr_blocks, nr_blocks_per_transfer, transfered_blocks[2];
    int buf, nxt_buf;   // indexul bufferului, 0 sau 1
    
    uint32_t block_range_start;
    uint32_t block_range_end;
    uint32_t task_block_range_start;
    uint32_t task_block_range_end;
    uint32_t msg;
    
    uint32_t size;
    uint32_t ea;
    
    nr_blocks =info->nr_img_blocks;
    nr_blocks_per_transfer = info->nr_blocks_per_transfer;

    struct block* c_blocks[2];
    struct pixel_block* pixel_blocks[2];
    
    // Allocate buffers
    c_blocks[0] = _spu_alloc(nr_blocks_per_transfer * sizeof(struct block));
    c_blocks[1] = _spu_alloc(nr_blocks_per_transfer * sizeof(struct block));
    pixel_blocks[0] = _spu_alloc(nr_blocks_per_transfer * sizeof(struct pixel_block));
    pixel_blocks[1] = _spu_alloc(nr_blocks_per_transfer * sizeof(struct pixel_block));
    
    
    // Receive task (block range to process)
    while (spu_stat_in_mbox()<=0);
    task_block_range_start = spu_read_in_mbox();
    
    while (spu_stat_in_mbox()<=0);
    task_block_range_end = spu_read_in_mbox();
    
    // Just a way to write things nicer. goto is appropriate in this case.
    if(task_block_range_start >= task_block_range_end)
        goto end;
    

    block_range_start = task_block_range_start;
    buf = 0;
    
    // First transfer, ouside the while loop
    block_range_end = block_range_start + nr_blocks_per_transfer < task_block_range_end ?
                      block_range_start + nr_blocks_per_transfer : task_block_range_end;
    transfered_blocks[buf] = block_range_end - block_range_start;
    
    size = transfered_blocks[buf] * sizeof(struct pixel_block);
    ea = (uint32_t)(info->image.pixel_blocks) + block_range_start * sizeof(struct pixel_block);
    mfc_getb((void *)pixel_blocks[buf], ea, size, tag_id[buf], 0, 0);
    

    block_range_start += info->nr_blocks_per_transfer;
    while(block_range_start < task_block_range_end)
    {
        // Get next buffer
        nxt_buf = buf^1;
        
        block_range_end = block_range_start + nr_blocks_per_transfer < task_block_range_end ?
                          block_range_start + nr_blocks_per_transfer : task_block_range_end;
        transfered_blocks[nxt_buf] = block_range_end - block_range_start;
    
        size = transfered_blocks[nxt_buf] * sizeof(struct pixel_block);
        ea = (uint32_t)(info->image.pixel_blocks) + block_range_start * sizeof(struct pixel_block);
        mfc_getb((void *)pixel_blocks[nxt_buf], ea, size, tag_id[nxt_buf], 0, 0);
        
        // Wait for previously requested buffer
        waitag(tag_id[buf]);
        
        // Process previously requested buffer
        compress(c_blocks[buf], pixel_blocks[buf], transfered_blocks[buf]);
        
        // Transfer compressed blocks (precedent)
        size = transfered_blocks[buf] * sizeof(struct block);
        ea = (uint32_t)(info->c_image.blocks) +
             (block_range_start - info->nr_blocks_per_transfer) * sizeof(struct block);
        mfc_put((void *)c_blocks[buf], ea, size, tag_id[buf], 0, 0);
        
        buf = nxt_buf;
        block_range_start += info->nr_blocks_per_transfer;
    }
    
    // Process last buffer
    waitag(tag_id[buf]);
    
    compress(c_blocks[buf], pixel_blocks[buf], transfered_blocks[buf]);
    
    // Transfer compressed blocks (precedent)
    size = transfered_blocks[buf] * sizeof(struct block);
    ea = (uint32_t)(info->c_image.blocks) +
         (block_range_start - info->nr_blocks_per_transfer) * sizeof(struct block);
    mfc_putb((void *)c_blocks[buf], ea, size, tag_id[buf], 0, 0);
    
    waitag(tag_id[buf]);
    
    end:
    
    // Send reply - finished compression
    msg = FINISHED_PHASE;
    
    while((spu_stat_out_mbox() <= 0));
    spu_write_out_mbox(msg);
    
    free_align(c_blocks[0]);
    free_align(c_blocks[1]);
    free_align(pixel_blocks[0]);
    free_align(pixel_blocks[1]);
}

void decompress_double_buffering_dma(uint32_t* tag_id, Info* info)
{
    unsigned int nr_blocks_per_transfer, nr_blocks, transfered_blocks[2];
    int buf, nxt_buf;   // indexul bufferului, 0 sau 1
    
    uint32_t block_range_start;
    uint32_t block_range_end;
    uint32_t task_block_range_start;
    uint32_t task_block_range_end;
    uint32_t msg;
    
    uint32_t size;
    uint32_t ea;
    
    nr_blocks = info->nr_img_blocks;
    nr_blocks_per_transfer = info->nr_blocks_per_transfer;
        
    struct block* c_blocks[2];
    struct pixel_block* pixel_blocks[2];
    
    // Allocate buffers
    c_blocks[0] = _spu_alloc(nr_blocks_per_transfer * sizeof(struct block));
    c_blocks[1] = _spu_alloc(nr_blocks_per_transfer * sizeof(struct block));
    pixel_blocks[0] = _spu_alloc(nr_blocks_per_transfer * sizeof(struct pixel_block));
    pixel_blocks[1] = _spu_alloc(nr_blocks_per_transfer * sizeof(struct pixel_block));

    // Receive task (block range to process)
    while (spu_stat_in_mbox()<=0);
    task_block_range_start = spu_read_in_mbox();
    
    while (spu_stat_in_mbox()<=0);
    task_block_range_end = spu_read_in_mbox();
    
    if(task_block_range_start >= task_block_range_end)
        goto end;
    
    block_range_start = task_block_range_start;
    buf = 0;
    
    // Primul transfer de date, in afara buclei
    block_range_end = block_range_start + nr_blocks_per_transfer < task_block_range_end ?
                      block_range_start + nr_blocks_per_transfer : task_block_range_end;
    transfered_blocks[buf] = block_range_end - block_range_start;
    
    // Get blocks to process
    size = transfered_blocks[buf] * sizeof(struct block);
    ea = (uint32_t)(info->c_image.blocks) + block_range_start * sizeof(struct block);
    mfc_getb((void *)c_blocks[buf], ea, size, tag_id[buf], 0, 0);
    
    block_range_start += info->nr_blocks_per_transfer;
    
    while(block_range_start < task_block_range_end)
    {
        nxt_buf = buf^1;
        block_range_end = block_range_start + nr_blocks_per_transfer < task_block_range_end ?
                          block_range_start + nr_blocks_per_transfer : task_block_range_end;
        transfered_blocks[nxt_buf] = block_range_end - block_range_start;
        
        // Get blocks to process
        size = transfered_blocks[nxt_buf] * sizeof(struct block);
        ea = (uint32_t)(info->c_image.blocks) + block_range_start * sizeof(struct block);
        mfc_getb((void *)c_blocks[nxt_buf], ea, size, tag_id[nxt_buf], 0, 0);
        
        waitag(tag_id[buf]);
        
        decompress(c_blocks[buf], pixel_blocks[buf], transfered_blocks[buf]);
        
        
        // Transfer decompressed pixels
        size = transfered_blocks[buf] * sizeof(struct pixel_block);
        ea = (uint32_t)(info->image2.pixel_blocks) +
             (block_range_start - info->nr_blocks_per_transfer) * sizeof(struct pixel_block);
        mfc_put((void *)pixel_blocks[buf], ea, size, tag_id[buf], 0, 0);

        buf = nxt_buf;
        block_range_start += info->nr_blocks_per_transfer;
    }

    // Astept ultimul buffer de date de la PPU
    waitag(tag_id[buf]);
    
    decompress(c_blocks[buf], pixel_blocks[buf], transfered_blocks[buf]);
    
    // Transfer decompressed pixels
    size = transfered_blocks[buf] * sizeof(struct pixel_block);
    ea = (uint32_t)(info->image2.pixel_blocks) + 
         (block_range_start - info->nr_blocks_per_transfer) * sizeof(struct pixel_block);
    mfc_putb((void *)pixel_blocks[buf], ea, size, tag_id[buf], 0, 0);
    waitag(tag_id[buf]);
    
    end:
    
    // Send reply - finished decompression
    msg = FINISHED_PHASE;
    
    while((spu_stat_out_mbox() <= 0));
    spu_write_out_mbox(msg);
    
    free_align(c_blocks[0]);
    free_align(c_blocks[1]);
    free_align(pixel_blocks[0]);
    free_align(pixel_blocks[1]);
}

int main(unsigned long long speid, unsigned long long argp, unsigned long long envp)
{
    Info info __attribute__ ((aligned(16)));

    uint32_t tag_id[2];
    tag_id[0] = mfc_tag_reserve();
    if (tag_id[0] == MFC_TAG_INVALID)
    {
        printf("SPU: ERROR can't allocate tag ID\n"); return -1;
    }
    tag_id[1] = mfc_tag_reserve();
    if (tag_id[1] == MFC_TAG_INVALID)
    {
        printf("SPU: ERROR can't allocate tag ID\n"); return -1;
    }

    // Initial DMA transfer -> get Info struct
    mfc_get((void *)&info, (unsigned int)argp, (uint32_t)envp, tag_id[0], 0, 0);
    waitag(tag_id[0]);
    
    // Get spu_id
    while (spu_stat_in_mbox()<=0);
    spu_id = spu_read_in_mbox();

    if(info.mode == 0)
    {
        compress_normal_dma(tag_id[0], &info);
        decompress_normal_dma(tag_id[0], &info);
    }
    else if(info.mode == 1)
    {
        compress_double_buffering_dma(tag_id, &info);
        decompress_double_buffering_dma(tag_id, &info);
    }

    return 0;
}


