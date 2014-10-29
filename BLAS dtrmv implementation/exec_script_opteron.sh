#!/bin/bash

[[ -z $COMPILER ]] && COMPILER="gcc"

if [[ $COMPILER="gcc" ]]; then
	mkdir -p data
	echo -e "#Size \t Time" > data/opteron_blas
	echo -e "#Size \t Time" > data/opteron_neoptimizat
	echo -e "#Size \t Time" > data/opteron_optimizat
	echo -e "#Size \t Time" > data/opteron_optimizat_flags

	for size in 1000 2000 4000 8000 12000 16000 20000 24000
	do
		./opteron_main blas $size >> data/opteron_blas
		./opteron_main neoptimizat $size >> data/opteron_neoptimizat
		./opteron_main optimizat $size >> data/opteron_optimizat
		./opteron_main_flags optimizat $size >> data/opteron_optimizat_flags
	done
fi
