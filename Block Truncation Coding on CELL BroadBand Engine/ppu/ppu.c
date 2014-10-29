// Author: Catalin Constantin Usurelu
// Grupa: 333CA

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libspe2.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>

#include "btc.h"
#include "ReusableBarrier.h"

#define MAX_SPU_THREADS 16


extern spe_program_handle_t spu;


// Strucutra cu parametrii trimisi threadurilor
// care se ocupa cu partea de comunicare
typedef struct comm_thread_arg_t
{
    spe_context_ptr_t ctx;
    unsigned int spu_id;
    unsigned int nr_spus;

    ReusableBarrier* reusable_barrier;
    
    unsigned int nr_img_blocks;
    
    Phase* phase;
    Info* info;
} comm_thread_arg_t;

// Strucutra cu parametrii trimisi threadurilor
// care se ocupa cu pornirea SPU-urilor
typedef struct spu_thread_arg_t
{
    spe_context_ptr_t ctx;
    uint8_t spu_id;
    Info* info;
} spu_thread_arg_t;

/*
 * thread used to start the programs on the SPE's
 */
void *spu_pthread_function(void* argument)
{
    spu_thread_arg_t arg = *(spu_thread_arg_t*)argument;

    unsigned int entry = SPE_DEFAULT_ENTRY;

    if (spe_context_run(arg.ctx, &entry, 0, (void*) arg.info, (void*) sizeof(Info), NULL)
            < 0)
    {
        perror("Failed running context");
        exit(1);
    }

    pthread_exit(NULL);
}

/*
 * threads used to keep communication with the SPE's
 */
void *comm_pthread_function(void* argument)
{
    comm_thread_arg_t arg = *(comm_thread_arg_t*) argument;
    unsigned int spu_id = arg.spu_id;
    unsigned int msg;

    unsigned int nr_img_blocks = arg.info->nr_img_blocks;
    unsigned int task_block_range_start;
    unsigned int task_block_range_end;
    unsigned int task_size;
    
    // Send initial -> SPU id
    msg = arg.spu_id;
    spe_in_mbox_write(arg.ctx, &msg, 1, SPE_MBOX_ANY_NONBLOCKING);
    
    task_size = nr_img_blocks / arg.nr_spus;
    
    // Edge case : in case we less blocks than SPUs, the first SPUs
    // receive a block to process while the rest receive 0 blocks
    if(nr_img_blocks <= arg.nr_spus)
    {
        task_block_range_start = arg.spu_id;
        task_block_range_end = arg.spu_id + 1 <= arg.nr_spus ? arg.spu_id + 1 : task_block_range_start;
    }
    else
    {
        task_block_range_start = arg.spu_id * task_size;
        task_block_range_end = (arg.spu_id + 1) * task_size;
    }
    
    // prevent round-off errors (last SPU might get less blocks than the rest)
    if(spu_id == arg.nr_spus)
    {
        task_block_range_end = nr_img_blocks;
    }

    while(1)
    {
        msg = task_block_range_start;
        spe_in_mbox_write(arg.ctx, &msg, 1, SPE_MBOX_ALL_BLOCKING);
        
        msg = task_block_range_end;
        spe_in_mbox_write(arg.ctx, &msg, 1, SPE_MBOX_ALL_BLOCKING);
        
        // Receive information from SPU (finished task)
        while (spe_out_mbox_status(arg.ctx) == 0);
        spe_out_mbox_read(arg.ctx, &msg, 1);
    
        // Am terminat o faza a algoritmului -> trecem la urmatoarea
        if(msg == FINISHED_PHASE)
        {
            ReusableBarrier_wait(arg.reusable_barrier);
            
            if(spu_id == 0) // Leader
            {
                next_phase(arg.phase);
            }
    
            // Wait to start next phase
            ReusableBarrier_wait(arg.reusable_barrier);
    
            // We finished compression + descompression
            if(*(arg.phase) == FINISHED)
            {
                pthread_exit(NULL);
            }
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char** argv)
{
    if (argc != 6)
    {
        printf("Usage: %s mod nr_spus in.pgm out.btc out.pgm\n", argv[0]);
        return 0;
    }
    
    unsigned int mode = atoi(argv[1]);
    unsigned int num_spu_threads = atoi(argv[2]);
    unsigned int i;

    spe_context_ptr_t ctxs[MAX_SPU_THREADS];
    pthread_t spu_threads[MAX_SPU_THREADS];
    pthread_t comm_threads[MAX_SPU_THREADS];
    
    Info info;
    Phase phase;
    comm_thread_arg_t comm_thread_args[MAX_SPU_THREADS];
    spu_thread_arg_t spu_thread_args[MAX_SPU_THREADS];
    
    ReusableBarrier reusable_barrier; 

    struct timeval t1, t2, t3, t4;
    double total_time = 0, scale_time = 0;
    
    
    gettimeofday(&t3, NULL);
    
    read_pgm(argv[3], &info.image); 
    
    gettimeofday(&t1, NULL);
    
    
    // Initializari
    
    ReusableBarrier_init(&reusable_barrier, num_spu_threads);
    
    info.nr_img_blocks = (info.image.width * info.image.height) / (BLOCK_SIZE * BLOCK_SIZE);
    info.nr_blocks_per_transfer = 16000 / sizeof(struct block);
    info.mode = mode;
    
    info.c_image.blocks = _alloc(info.nr_img_blocks * sizeof(struct block));
    info.image2.pixel_blocks = _alloc(info.nr_img_blocks * sizeof(struct pixel_block));
    
    // Initial phase
    phase = COMPRESS;
    
    // We do this in PPU, the SPU's are strictly for compressing/decompressing
    info.c_image.width = info.image.width;
    info.c_image.height = info.image.height;
    
    info.image2.width = info.image.width;
    info.image2.height = info.image.height;
    
    /* Create several SPE-threads to execute 'SPU'. */
    for (i = 0; i < num_spu_threads; i++)
    {
        /* Create context */
        if ((ctxs[i] = spe_context_create(0, NULL)) == NULL)
        {
            perror("Failed creating context");
            exit(1);
        }

        /* Load program into context */
        if (spe_program_load(ctxs[i], &spu))
        {
            perror("Failed loading program");
            exit(1);
        }
        
        spu_thread_args[i].ctx = ctxs[i];
        spu_thread_args[i].spu_id = i;
        spu_thread_args[i].info = &info;

        /* Create thread for each SPE context */
        if (pthread_create(&spu_threads[i], NULL, &spu_pthread_function, spu_thread_args + i))
        {
            perror("Failed creating thread");
            exit(1);
        }
    }

    for (i = 0; i < num_spu_threads; i++)
    {
        comm_thread_args[i].ctx = ctxs[i];
        comm_thread_args[i].spu_id = i;
        comm_thread_args[i].nr_spus = num_spu_threads;
        comm_thread_args[i].reusable_barrier = &reusable_barrier;
        comm_thread_args[i].info = &info;
        comm_thread_args[i].phase = &phase;

        if (pthread_create(&comm_threads[i], NULL, &comm_pthread_function,
                comm_thread_args + i))
        {
            perror("Failed creating thread");
            exit(1);
        }
    }
    
    
    /* Wait for comm-thread to complete execution. */
    for (i = 0; i < num_spu_threads; i++)
    {
        if (pthread_join(comm_threads[i], NULL))
        {
            perror("Failed pthread_join");
            exit(1);
        }
    }
    
    
    /* Wait for SPU-thread to complete execution. */
    for (i = 0; i < num_spu_threads; i++)
    {
        if (pthread_join(spu_threads[i], NULL))
        {
            perror("Failed pthread_join");
            exit(1);
        }
        

        /* Destroy context */
        if (spe_context_destroy(ctxs[i]) != 0)
        {
            perror("Failed destroying context");
            exit(1);
        }
    }

    // Finished compression+decompression
    gettimeofday(&t2, NULL);    
    
    
    write_btc(argv[4], &info.c_image);
    write_pgm(argv[5], &info.image2);

    // Cleanup
    free_btc(&info.c_image);
    free_pgm(&info.image);
    free_pgm(&info.image2);
    
    ReusableBarrier_destroy(&reusable_barrier);

    
    // Finished everything
    gettimeofday(&t4, NULL);

    
    total_time += GET_TIME_DELTA(t3, t4);
    scale_time += GET_TIME_DELTA(t1, t2);

    printf("Encoding / Decoding time: %lf\n", scale_time);
    printf("Total time: %lf\n", total_time);

    return 0;
}
