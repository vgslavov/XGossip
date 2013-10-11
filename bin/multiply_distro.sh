#!/bin/sh

if [ $# -ne 3 ]; then
	echo "Usage: $0 <src> <dst> <times>"
	exit 1;
fi

SRC=$1
DST=$2
TIMES=$3

n=1
while [ $n -le $TIMES ] ; do
	for i in `ls $SRC` ; do
		cp -R $SRC/$i $DST/$i-$n
	done
	n=`expr $n + 1`
done
