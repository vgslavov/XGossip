#!/bin/bash

if [ $# -ne 2 ]; then
        echo "Usage: $0 <gpsilog dir> <results dir>"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
TYPE="gpsi"
LOGDIR=${1}
RESULTSDIR=${2}
#TEAMLOG=${3}
GREPSTRING="queryresultend: rxqseq:"

if [ $DEBUG = 1 ]; then
	set -x
fi

skipped=0
for i in `ls $LOGDIR/*.$TYPE` ; do
	host=`basename $i .$TYPE`
	#echo $host
	#var=`grep -m 1 'host' $LOGFILE`
	# everything to the right of the rightmost colon
	#host=${var##*: }

	#if [ ! -d $RESULTSDIR/$host ] ; then
		#mkdir $RESULTSDIR/$host
	#fi

	nresults=`grep -c "queryresultend:" $i`
	#echo $nresults

	n=0
	while [ $n -lt $nresults ]; do
		results=`grep "$GREPSTRING $n " $i`
		set -- `echo $results | awk '{print $5, $7}'`
		sigmatches=$1
		totalavg=$2

		# skip if teamID is not in teamlog
		#ret=`grep -c $teamID $TEAMLOG`
		#if [ $ret -eq 0 ]; then
			#skipped=`expr $skipped + 1`
			#echo $skipped teamIDs skipped
			#n=`expr $n + 1`
			#continue
		#fi

		# sigs + 2 headers + 1 sum
		#grep -A `expr $listsize + 3` "list T_0: txseq: $nseq teamID(i=$n)" $i > $RESULTSDIR/$host/$teamID.$nseq
		# sigs + header + rxmsglen line
		grep -B `expr $sigmatches + 2` "$GREPSTRING $n " $i > $RESULTSDIR/$host.rxqseq-$n.results
		n=`expr $n + 1`
	done
done

#echo $skipped teamIDs skipped
