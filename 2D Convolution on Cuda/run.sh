#!/bin/bash
#
# Author: Heri
#
# Script de submitere a job-urilor pe fiecare coda, folosind compilatoare diferite
#

mprun.sh --job-name cuda --queue ibm-dp.q \
	--modules "libraries/cuda-5.0 " \
	--script run_tests.sh --show-qsub --show-script --batch-job
