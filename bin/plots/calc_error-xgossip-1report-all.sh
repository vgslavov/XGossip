#!/bin/bash

if [ $# -ne 6 ]; then
	echo "Usage: $0 [lists dir] [allsig log] [1report] <rounds> <minfreq> [teamidnotfound log]"
	exit 1;
fi

USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
LISTDIR=${1}
ALLSIGLOG=${2}
REPORT=${3}
# total rounds to process
ROUNDS=${4}
MINFREQ=${5}
TEAMIDNOTFOUNDLOG=${6}
# special script for 1report
CALCERROR="$HOMEDIR/bin/plots/calc_error-xgossip-1report.sh"

#set -x

# TODO: make arg?
STARTROUND=5

nseq=$STARTROUND
while [ $nseq -lt $ROUNDS ] ; do
	echo "peer,teamid,churn,sig,estimated count,true count,relative error" >> $REPORT.$nseq.csv
	nseq=`expr $nseq + 1`
done

nseq=$STARTROUND
for i in `ls -d $LISTDIR/*` ; do
	HOST=`basename $i`
	echo "HOST: $HOST"
	while [ $nseq -lt $ROUNDS ] ; do
		echo "seq: $nseq"
		$CALCERROR $i $ALLSIGLOG $REPORT.$nseq.csv $nseq $MINFREQ $TEAMIDNOTFOUNDLOG
		nseq=`expr $nseq + 1`
	done
	nseq=$STARTROUND
done
