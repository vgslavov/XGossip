#!/bin/bash

if [ $# -ne 5 ]; then
	echo "Usage: $0 [results dir] [allquery log] [new report] <clusternodes> <peers>"
	exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
RESULTSDIR=${1}
ALLQUERYLOG=${2}
NEWREPORT=${3}
NDATASETS=${4}
PEERS=${5}
# scale for bc
SCL=6

if [ $DEBUG = 1 ]; then
	set -x
fi

echo "qid,dtd,query sig,sig matches: est,sig matches: true,sig matches: rel error (%),freq: est,freq (*peers): est,freq: true,freq (*instances): true,freq: rel error (%)" > $NEWREPORT

sigmatches=0
totalavg=0
n=0
for i in `ls $RESULTSDIR`; do
	# get estimate matches
	set -- `grep -m 1 "querysig:" $RESULTSDIR/$i | awk '{print $4, $6, $8}'`
	qid=$1
	querysig=$2
	dtd=$3
	set -- `grep -m 1 "queryresultend:" $RESULTSDIR/$i | awk '{print $5, $7}'`
	sigmatches=$1
	totalavg=$2
	# avg * PEERS = sum!
	totalavgn=`echo "scale=$SCL; $2 * $PEERS" | bc`

	# get true matches
	# gpsi uses 2 formats
	#set -- `grep -m 1 "querysig: $querysig sigmatches: " $ALLQUERYLOG | awk '{print $6, $8}'`
	set -- `grep -m 1 ",$querysig," $ALLQUERYLOG | awk -F "," '{print $4, $5}'`
	truesigmatches=$1
	truefreq=$2
	# there was a full dataset on EACH instance
	truefreqn=`expr $2 \* $NDATASETS`
	echo -n "$qid,$dtd,$querysig,$sigmatches,$truesigmatches,," >> $NEWREPORT
	echo -n "$totalavg,$totalavgn,$truefreq,$truefreqn," >> $NEWREPORT
	echo >> $NEWREPORT
done
