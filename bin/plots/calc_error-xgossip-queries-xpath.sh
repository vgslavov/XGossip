#!/bin/bash

if [ $# -ne 4 ]; then
	echo "Usage: $0 [gpsi report] [allquery log] [new report] <clusternodes>"
	exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
GPSIREPORT=${1}
ALLQUERYLOG=${2}
NEWREPORT=${3}
NDATASETS=${4}
# scale for bc
SCL=6

if [ $DEBUG = 1 ]; then
	set -x
fi

echo "qid,query sig,sig matches (est),total avg (est),sig matches (true),total freq (true)" > $NEWREPORT

sigmatches=0
totalavg=0
n=0
while read line; do
	if [ $n -eq 0 ]; then
		n=`expr $n + 1`
		continue
	fi
	#echo $line
	set -- `echo $line | awk -F "," '{print $1, $2, $3, $4 }'`
	qid=$1
	querysig=$2
	sigmatches=$3
	totalavg=$4
	#teamID=$5
	set -- `grep -m 1 "querysig: $querysig sigmatches: " $ALLQUERYLOG | awk '{print $6, $8}'`
	truesigmatches=`expr $1 \* $NDATASETS`
	truefreq=`expr $2 \* $NDATASETS`
	echo -n "$qid,$querysig,$sigmatches,$totalavg," >> $NEWREPORT
	echo -n "$truesigmatches,$truefreq," >> $NEWREPORT
	echo >> $NEWREPORT
done < "$GPSIREPORT"
