#!/bin/bash

if [ $# -ne 2 ]; then
        echo "Usage: $0 [src dir] [absolute dst dir]"
        exit 1;
fi

SRCDIR=$1
DSTDIR=$2

for i in `ls $SRCDIR/*.tgz`; do
	echo $i
        cd $DSTDIR; tar zxf $i
done
