#!/bin/sh

if [ $# -ne 3 ]; then
	echo "Usage: $0 <src> <dst> <hosts>"
	exit 1;
fi

SRC=$1
DST=$2
HOSTS=$3
TOTALSIGS=`ls $SRC|wc -l`
PERHOST=`expr $TOTALSIGS \/ $HOSTS`
#PERHOST=5

n=1
while [ $n -le $HOSTS ] ; do
	mkdir $DST/$n
	n=`expr $n + 1`
done

echo $TOTALSIGS
echo $PERHOST

n=1
HOSTNUM=1
# i is a sig file
for i in `ls $SRC` ; do
	if [ $n -gt $PERHOST ] ; then
		HOSTNUM=`expr $HOSTNUM + 1`
		n=1
	fi
	cp $SRC/$i $DST/$HOSTNUM/
	n=`expr $n + 1`
done
