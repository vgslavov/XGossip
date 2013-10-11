#!/bin/sh

if [ $# -ne 7 ]; then
        echo "Usage: $0 <listdir> <allsiglog> <reportdir> <rounds> <minfreq> <clusternodes> <peers>"
        exit 1;
fi

LISTDIR=$1
ALLSIGLOG=$2
REPORTDIR=$3
ROUNDS=$4
MINFREQ=$5
NDATASETS=$6
PEERS=$7
CALCERROR="/home/vsfgd/bin/plots/calc_error-vanilla.sh"

for i in `ls $LISTDIR` ; do
	echo "host: $i"
	$CALCERROR $LISTDIR/$i $ALLSIGLOG $REPORTDIR/$i.csv $ROUNDS $MINFREQ $NDATASETS $PEERS
done

