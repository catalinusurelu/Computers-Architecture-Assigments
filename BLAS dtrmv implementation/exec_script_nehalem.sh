#!/bin/bash

[[ -z $COMPILER ]] && COMPILER="gcc"

if [[ $COMPILER="gcc" ]]; then
	mkdir -p data
	echo -e "#Size \t Time" > data/nehalem_blas
	echo -e "#Size \t Time" > data/nehalem_neoptimizat
	echo -e "#Size \t Time" > data/nehalem_optimizat
	echo -e "#Size \t Time" > data/nehalem_optimizat_flags

	for size in 1000 2000 4000 8000 12000 16000 20000 24000 28000
	do
		./nehalem_main blas $size >> data/nehalem_blas
		./nehalem_main neoptimizat $size >> data/nehalem_neoptimizat
		./nehalem_main optimizat $size >> data/nehalem_optimizat
		./nehalem_main_flags optimizat $size >> data/nehalem_optimizat_flags
	done
fi
