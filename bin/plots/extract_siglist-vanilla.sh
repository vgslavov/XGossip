#!/bin/bash

if [ $# -ne 3 ]; then
        echo "Usage: $0 <gpsi log dir> <lists dir> [rounds]"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
TYPE="gpsi"
LOGDIR=${1}
LISTDIR=${2}
# TODO: calculate from log
ROUNDS=${3}

if [ $DEBUG = 1 ]; then
	set -x
fi

for i in `ls $LOGDIR/*.$TYPE` ; do
	host=`basename $i .$TYPE`

	if [ ! -d $LISTDIR/$host ] ; then
		mkdir $LISTDIR/$host
	fi

	nseq=-1
	while [ $nseq -le $ROUNDS ]; do
		list=`grep "list T_0: txseq: $nseq " $i`
		set -- `echo $list | awk '{print $6}'`
		listsize=$1
		# sig + 2 headers + 1 sum
		grep -A `expr $listsize + 3` "list T_0: txseq: $nseq " $i > $LISTDIR/$host/list.$nseq
		nseq=`expr $nseq + 1`
	done
done
