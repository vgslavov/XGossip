#!/bin/bash

if [ $# -ne 4 ]; then
	echo "Usage: $0 [gpsireport] [allsiglog] [newreport] <clusternodes>"
	exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
GPSIREPORT=${1}
ALLSIGLOG=${2}
NEWREPORT=${3}
NDATASETS=${4}
# scale for bc
SCL=6

if [ $DEBUG = 1 ]; then
	set -x
fi

echo "id,query sig,total avg (/teamsize),total avg (est),total freq (true)" > $NEWREPORT

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
	id=$1
	sig=$2
	totalavgteam=$3
	totalavg=$4
	#teamID=$5
	truefreq=`grep -m 1 -e "^sig[0-9]*: $sig " $ALLSIGLOG | awk '{print $5}'`
	truefreq=`expr $truefreq \* $NDATASETS`
	echo -n "$id,$sig,$totalavgteam,$totalavg," >> $NEWREPORT
	echo -n "$truefreq," >> $NEWREPORT
	echo >> $NEWREPORT
done < "$GPSIREPORT"
