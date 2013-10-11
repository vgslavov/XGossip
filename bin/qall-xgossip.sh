#!/bin/sh

BOOTSTRAPHOST="hosts.bootstrap"

#DATADIR='/home/raopr/cs5590ld/P2PXML'
#DTDPATH="$DATADIR/DTDs"
#XMLPATH="$DATADIR/data"
# because DTDs > XMLs
#DTDS=`cd $XMLPATH;ls -d *.dtd`
#XPATH="$DATADIR/XPathQueries"

if [ $# -lt 2 -o $# -gt 3 ]; then
        echo "Usage: $0 <dirsigs> <num-per-dir> [host]"
        exit 1;
fi

DIRSIGS=$1
NUM=$2

if [ $# -eq 3 ]; then
        HOST=$3
elif [ -f $BOOTSTRAPHOST ]; then
	HOST=`cat $BOOTSTRAPHOST`        
	echo "HOST: $HOST"
else
        echo "no host available"
        exit 127
fi


n=0
for i in `ls $DIRSIGS` ; do
        for j in `ls $DIRSIGS/$i` ; do
		echo "sigfile: $i/$j"
		./query-xgossip.sh $DIRSIGS/$i/$j $HOST
		n=`expr $n + 1`
		if [ $n -eq $NUM ] ; then
			break;
		fi
        done
	n=0
done

