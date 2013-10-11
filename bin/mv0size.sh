#!/bin/sh

if [ $# -ne 2 ]; then
	echo "Usage: $0 <src> <dst>"
	exit 1;
fi

SRC=$1
DST=$2
#TMP=$3

n=0
for i in `ls $SRC` ; do
	#for j in `ls $SRC/$i` ; do
		#if [ -s $SRC/$i/$j ]; then
		if [ -s $SRC/$i ]; then
			n=`expr $n + 1`
		else
			#mv $SRC/$i/$j $DST
			mv $SRC/$i $DST
		fi
	#done
done
echo $n
