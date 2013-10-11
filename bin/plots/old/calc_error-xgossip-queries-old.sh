#!/bin/bash

if [ $# -ne 7 ]; then
	echo "Usage: $0 [gpsilogdir] [allquerylog] [report] [teamidnotfoundlog] <nqueries> <teamsize> <clusternodes>"
	exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
GPSILOGDIR=${1}
ALLQUERYLOG=${2}
REPORT=${3}
TEAMIDNOTFOUNDLOG=${4}
NQUERIES=${5}
TEAMSIZE=${6}
NDATASETS=${7}
TYPE="gpsi"
# scale for bc
SCL=6

if [ $DEBUG = 1 ]; then
	set -x
fi

echo "qid,query sig,teamID,sig matches (est),total avg (est),sig matches (true),total freq (true)" > $REPORT

#queryresult: teamID found qid: $qid querysig: $querysig sigmatches: $sigmatches totalavg: $totalavg teamID: $teamID
n=0
while [ $n -lt $NQUERIES ]; do
	# TODO: inefficient (don't write to disk)
	#ret=`grep "queryresult: teamID NOT found qid: $n " $GPSILOGDIR/*.$TYPE > $TMPFILE; echo $?`
	ret=`grep "queryresult: teamID found qid: $n " $GPSILOGDIR/*.$TYPE > $TMPFILE; echo $?`
	#queryres=0
	#queryres=`grep "queryresult: teamID found qid: $n " $GPSILOGDIR/*.$TYPE`
	#if [ -z "$queryres" ]; then
	if [ $ret -ne 0 ]; then
		echo "skipping qid: $n"
		n=`expr $n + 1`
		continue
	else
		echo "found qid: $n"
	fi

	sumsigmatches=0
	sumtotalavg=0
	while read line; do
		sigmatches=0
		totalavg=0
		#echo $line
		#set -- `echo $line | awk '{print $6, $8, $10, $12, $14 }'`
		set -- `echo $line | awk '{print $5, $7, $9, $11, $13 }'`
		qid=$1
		querysig=$2
		sigmatches=$3
		totalavg=$4
		teamID=$5
		# TODO: inneficient (don't do the same lookup multiple times)
		# don't need more than 1 lines (all are the same)
		set -- `grep -m 1 "querysig: $querysig sigmatches: " $ALLQUERYLOG | awk '{print $6, $8}'`
		truesigmatches=`expr $1 \* $NDATASETS`
		truefreq=`expr $2 \* $NDATASETS`
		if [ $sigmatches -eq 0 ]; then
			continue
		fi
		sigmatches=`echo "scale=$SCL; $sigmatches * $TEAMSIZE" | bc`
		totalavg=`echo "scale=$SCL; $totalavg * $TEAMSIZE" | bc`
		sumsigmatches=`expr $sumsigmatches + $sigmatches`
		sumtotalavg=`echo "scale=$SCL; $sumtotalavg + $totalavg" | bc`
		echo -n "$qid,$querysig,$teamID,$sigmatches,$totalavg," >> $REPORT
		echo -n "$truesigmatches,$truefreq," >> $REPORT
		echo >> $REPORT
	done < "$TMPFILE"

	echo "$qid,$querysig,all,$sumsigmatches,$sumtotalavg,$truesigmatches,$truefreq" >> $REPORT

	n=`expr $n + 1`
done
