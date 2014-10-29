#!/bin/bash

[[ -z $COMPILER ]] && COMPILER="gcc"

if [[ $COMPILER="gcc" ]]; then
	mkdir -p data
	echo -e "#Size \t Time" > data/quad_blas
	echo -e "#Size \t Time" > data/quad_neoptimizat
	echo -e "#Size \t Time" > data/quad_optimizat
	echo -e "#Size \t Time" > data/quad_optimizat_flags

	for size in 1000 2000 4000 8000 12000 16000 20000 24000 28000
	do
		./quad_main blas $size >> data/quad_blas
		./quad_main neoptimizat $size >> data/quad_neoptimizat
		./quad_main optimizat $size >> data/quad_optimizat
		./quad_main_flags optimizat $size >> data/quad_optimizat_flags
	done
fi
