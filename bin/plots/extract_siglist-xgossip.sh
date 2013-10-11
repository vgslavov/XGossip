#!/bin/bash

if [ $# -ne 3 ]; then
        echo "Usage: $0 <gpsi log dir> <lists dir> <junk>"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
TYPE="gpsi"
#LOGFILE=${1}
LOGDIR=${1}
LISTDIR=${2}
TEAMLOG=${3}

if [ $DEBUG = 1 ]; then
	set -x
fi

skipped=0
for i in `ls $LOGDIR/*.$TYPE` ; do
	host=`basename $i .$TYPE`
	#var=`grep -m 1 'host' $LOGFILE`
	# everything to the right of the rightmost colon
	#host=${var##*: }

	if [ ! -d $LISTDIR/$host ] ; then
		mkdir $LISTDIR/$host
	fi

	nseq=-1
	while [ 1 ]; do
		# move to the next file if no more seq in this one
		# space after $nseq is important!
		listsinseq=`grep -c "list T_0: txseq: $nseq " $i`
		if [ $listsinseq -eq 0 ]; then
			break
		fi

		n=0
		while [ $n -lt $listsinseq ]; do
			list=`grep "list T_0: txseq: $nseq teamID(i=$n)" $i`
			set -- `echo $list | awk '{print $6, $8}'`
			teamID=$1
			listsize=$2
			#ret=`grep -c $teamID $TEAMLOG`
			ret=1
			# skip if teamID is not in teamlog
			if [ $ret -eq 0 ]; then
				skipped=`expr $skipped + 1`
				#echo $skipped teamIDs skipped
				n=`expr $n + 1`
				continue
			fi
			# sig + 2 headers + 1 sum
			grep -A `expr $listsize + 3` "list T_0: txseq: $nseq teamID(i=$n)" $i > $LISTDIR/$host/$teamID.$nseq
			n=`expr $n + 1`
		done
		nseq=`expr $nseq + 1`
	done
done

echo $skipped teamIDs skipped
