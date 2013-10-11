#!/bin/sh

if [ $# -ne 2 ] ; then
	echo "Usage: $0 <file> <parts>"
	exit 1;
fi

#set -x

FILE=$1
PARTS=$2
TOTAL=`wc -l $FILE | awk '{print $1}'`
PERPART=`expr $TOTAL \/ $PARTS`
SPLIT=$PERPART

echo $PERPART

n=0
npart=0
for i in `cat $FILE`; do
	if [ $n -eq $SPLIT ] ; then
		npart=`expr $npart + 1`
		SPLIT=`expr $SPLIT + $PERPART`
	fi
	echo $i >> $FILE.$npart
	n=`expr $n + 1`
done
