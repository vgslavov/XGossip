#!/bin/sh

if [ $# -ne 4 ]; then
	echo "Usage: $0 <initdir> <lshdir> <bands> <rows>"
	exit 1;
fi

USER=`whoami`
HOMEDIR="/home/$USER"
INITDIR=$1
LSHDIR=$2
BANDS=$3
ROWS=$4
IRRPOLYFILE="$HOMEDIR/bin/irrpoly-deg9.dat"
RANDPRIME=1122941
GPSI="$HOMEDIR/build/chord/tools/gpsi"

# i is dir with sigs for 1 host
for i in `ls $INITDIR` ; do
	$GPSI -H -E -P $INITDIR/$i -F $HOMEDIR/bin/hash.dat -d $RANDPRIME -j $IRRPOLYFILE -B $BANDS -R $ROWS -L $LSHDIR/`basename $i .init`.lsh
done
