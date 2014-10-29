#
# Author: Heri
# 
# CC=gcc

CC=gcc
CFLAGS=
DEFAULT_OPTIMIZATION=-O0
OPTIMIZATION_FLAGS=-O3 -march=native
LDFLAGS = -lcblas -latlas -lf77blas -llapack -lptcblas -lptf77blas -lm
MODULE_NEHALEM= libraries/atlas-3.10.1-gcc-4.4.6-nehalem 
MODULE_OPTERON= libraries/atlas-3.10.1-gcc-4.4.6-opteron 
MODULE_QUAD= libraries/atlas-3.10.1-gcc-4.4.6-quad
NEHALEM_INCLUDE = /opt/tools/libraries/atlas/3.10.1-nehalem-gcc-4.4.6/include
NEHALEM_LIBRARY = /opt/tools/libraries/atlas/3.10.1-nehalem-gcc-4.4.6/lib
OPTERON_INCLUDE = /opt/tools/libraries/atlas/3.10.1-opteron-gcc-4.4.6/include
OPTERON_LIBRARY = /opt/tools/libraries/atlas/3.10.1-opteron-gcc-4.4.6/lib
QUAD_INCLUDE = /opt/tools/libraries/atlas/3.10.1-quad-gcc-4.4.6/include
QUAD_LIBRARY = /opt/tools/libraries/atlas/3.10.1-quad-gcc-4.4.6/lib
SRC = main.c
NEHALEM_TARGET=nehalem_main
OPTERON_TARGET=opteron_main
QUAD_TARGET=quad_main
NEHALEM_TARGET_FLAGS=nehalem_main_flags
OPTERON_TARGET_FLAGS=opteron_main_flags
QUAD_TARGET_FLAGS=quad_main_flags

build: build_nehalem build_opteron build_quad build_nehalem_with_flags build_opteron_with_flags build_quad_with_flags

build_nehalem:
	$(CC) $(DEFAULT_OPTIMIZATION) -I$(NEHALEM_INCLUDE) -o $(NEHALEM_TARGET) $(SRC) -L$(NEHALEM_LIBRARY) $(LDFLAGS) $(CFLAGS)
build_opteron:
	$(CC) $(DEFAULT_OPTIMIZATION) -I$(OPTERON_INCLUDE) -o $(OPTERON_TARGET) $(SRC) -L$(OPTERON_LIBRARY) $(LDFLAGS) $(CFLAGS)
build_quad:
	$(CC) $(DEFAULT_OPTIMIZATION) -I$(QUAD_INCLUDE) -o $(QUAD_TARGET) $(SRC) -L$(QUAD_LIBRARY) $(LDFLAGS) $(CFLAGS)

build_nehalem_with_flags:
	$(CC) $(OPTIMIZATION_FLAGS) -I$(NEHALEM_INCLUDE) -o $(NEHALEM_TARGET_FLAGS) $(SRC) -L$(NEHALEM_LIBRARY) $(LDFLAGS) $(CFLAGS)
build_opteron_with_flags:
	$(CC) $(OPTIMIZATION_FLAGS) -I$(OPTERON_INCLUDE) -o $(OPTERON_TARGET_FLAGS) $(SRC) -L$(OPTERON_LIBRARY) $(LDFLAGS) $(CFLAGS)
build_quad_with_flags:
	$(CC) $(OPTIMIZATION_FLAGS) -I$(QUAD_INCLUDE) -o $(QUAD_TARGET_FLAGS) $(SRC) -L$(QUAD_LIBRARY) $(LDFLAGS) $(CFLAGS)


run: submit-job

submit-job:
	./run.sh

make-plot:
	mkdir -p plots
	gnuplot ./plot_nehalem_eps
	gnuplot ./plot_opteron_eps
	gnuplot ./plot_quad_eps
	gnuplot ./plot_all_eps

draw-plot: make-plot

clean:
	rm -rf $(NEHALEM_TARGET) $(OPTERON_TARGET) $(QUAD_TARGET) $(NEHALEM_TARGET_FLAGS) $(OPTERON_TARGET_FLAGS) $(QUAD_TARGET_FLAGS) *~
	rm -rf *blas *optimizat* *.eps out/* plots/* data/*
