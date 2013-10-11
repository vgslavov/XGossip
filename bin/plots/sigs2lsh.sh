#!/bin/sh

if [ $# -ne 4 ]; then
	echo "Usage: $0 <sigdir> <lshdir> <bands> <rows>"
	exit 1;
fi

USER=`whoami`
HOMEDIR="/home/$USER"
SIGDIR=$1
LSHDIR=$2
BANDS=$3
ROWS=$4
GPSI="$HOMEDIR/build/chord/tools/gpsi"

# i is dir with sigs for 1 host
for i in `ls $SIGDIR` ; do
	$GPSI -H -E -s $SIGDIR/$i/ -F $HOMEDIR/bin/hash.dat -d 1122941 -j $HOMEDIR/bin/irrpoly-deg9.dat -B $BANDS -R $ROWS -L $LSHDIR/log-$i.lsh
done
