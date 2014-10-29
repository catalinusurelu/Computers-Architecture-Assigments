#!/bin/bash
#
# Author: Heri
#
# Script de submitere a job-urilor pe fiecare coda, folosind compilatoare diferite
#

mprun.sh --job-name nehalem --queue ibm-nehalem.q \
	--modules "libraries/atlas-3.10.1-gcc-4.4.6-nehalem" \
	--script exec_script_nehalem.sh --show-qsub --show-script --batch-job
mprun.sh --job-name opteron --queue ibm-opteron.q \
	--modules "libraries/atlas-3.10.1-gcc-4.4.6-opteron" \
	--script exec_script_opteron.sh --show-qsub --show-script --batch-job
mprun.sh --job-name quad --queue ibm-quad.q \
	--modules "libraries/atlas-3.10.1-gcc-4.4.6-quad" \
	--script exec_script_quad.sh --show-qsub --show-script --batch-job
