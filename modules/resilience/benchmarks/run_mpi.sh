#!/bin/bash
#SBATCH -q debug
##SBATCH -q regular
#SBATCH -N 1
#SBATCH -C haswell
#SBATCH -t 00:30:00
##SBATCH -t 00:58:00

BIG=1;
NUM=5;

while getopts b:n:h option
do
  case "${option}"
  in
    b) BIG=${OPTARG};;
    n) NUM=${OPTARG};;
    h) echo "Options
             -b <0|1> default is 1 i.e use bigger inputs (same as paper) which might take more time
             -n <1..inf> default is 5 i.e. run each experiment 5 times
             -h show options"; exit 0;;
  esac
done

echo figure 4
echo use big inputs $BIG
echo number of times $NUM

cd stencil1d/mpi
make clean
make
sh run.sh $NUM $BIG
cd ../../

