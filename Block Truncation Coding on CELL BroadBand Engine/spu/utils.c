#include <stdio.h>
#include <stdlib.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include <libmisc.h>
#include <string.h>

#include "utils.h"


void* _spu_alloc(int size){
	void *res;

	res = malloc_align(size, 4);
	if (!res){
		fprintf(stderr, "%s: Failed to allocated %d bytes\n", __func__,
				size);
		exit(0);
	}

	return res;
}

