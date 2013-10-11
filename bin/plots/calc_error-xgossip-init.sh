#!/bin/bash

if [ $# -ne 5 ]; then
	echo "Usage: $0 [gpsi report] [allsig log] [new report] <clusternodes> <k>"
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
K=${5}
# scale for bc
SCL=6

if [ $DEBUG = 1 ]; then
	set -x
fi

echo "sig,total freq (init),total freq (true)" > $NEWREPORT

n=0
while read line; do
	#if [ $n -eq 0 ]; then
		#n=`expr $n + 1`
		#continue
	#fi
	#echo $line
	set -- `echo $line | awk '{print $2, $3 }'`
	sig=$1
	# (init est. freq) / k
	initfreq=`echo "scale=$SCL;  $2 / $K" | bc`
	truefreq=`grep -m 1 "^sig[0-9]*: $sig " $ALLSIGLOG | awk '{print $3}'`
	# (true freq) * instances
	truefreq=`expr $truefreq \* $NDATASETS`
	echo "$sig,$initfreq,$truefreq," >> $NEWREPORT
	#echo $sig
done < "$GPSIREPORT"
