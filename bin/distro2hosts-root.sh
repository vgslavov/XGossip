#!/bin/sh

if [ $# -ne 3 ]; then
	echo "Usage: $0 <src> <dst> <freq-per-peer file>"
	exit 1;
fi

SRC=$1
DST=$2
FILE=$3
SIGLIST=`ls $SRC`

#set -x

CURSIG=1
while read line; do
	set -- `echo $line | awk '{ print $2, $5 }'`
	peer=$1
	numsigs=$2
	mkdir $DST/$peer
	n=0
	while [ $n -lt $numsigs ] ; do
		cp $SRC/*.$CURSIG $DST/$peer
		n=`expr $n + 1`
		CURSIG=`expr $CURSIG + 1`
	done
done < "$FILE"
