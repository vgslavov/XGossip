#!/bin/sh

if [ $# -ne 5 ]; then
	echo "Usage: $0 <src> <dst> <hosts> <sig-per-distro> <sig-per-host>"
	exit 1;
fi

SRC=$1
DST=$2
HOSTS=$3
PERDISTRO=$4
PERHOST=$5
TOTALSIGS=`expr $PERHOST \* $HOSTS`

n=1
while [ $n -le $HOSTS ] ; do
	mkdir $DST/$n
	n=`expr $n + 1`
done

n=0
z=0
HOSTNUM=1
# i is a distro dir
for i in `ls $SRC` ; do
	# j is a sig file
	for j in `ls $SRC/$i` ; do
		if [ $n -lt $PERDISTRO ] ; then
			cp $SRC/$i/$j $DST/$HOSTNUM/
			n=`expr $n + 1`
		else
			HOSTNUM=`expr $HOSTNUM + 1`
			if [ $HOSTNUM -gt $HOSTS ] ; then
				HOSTNUM=1
			fi
			cp $SRC/$i/$j $DST/$HOSTNUM/
			n=1
		fi

		z=`expr $z + 1`
		if [ $z -eq $TOTALSIGS ] ; then
			exit
		fi
	done
done

if [ $z -ne $TOTALSIGS ] ; then
	echo "out of signatures"
fi
