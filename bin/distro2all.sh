#!/bin/sh

if [ $# -ne 3 ]; then
	echo "Usage: $0 <src> <dst> <tmp>"
	exit 1;
fi

SRC=$1
DST=$2
TMP=$3

n=1
# i is a distro dir?
for i in `ls $SRC` ; do
	# j is a sig file?
	for j in `ls $SRC/$i` ; do
		if [ -s $SRC/$i/$j ]; then
			#cp $SRC/$i/$j $DST/$j
			cp $SRC/$i/$j $DST/$j.$n
			n=`expr $n + 1`
		else
			mv $SRC/$i/$j $TMP
		fi
	done
done
