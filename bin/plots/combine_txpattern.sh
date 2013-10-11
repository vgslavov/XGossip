#!/bin/bash

if [ $# -ne 4 ]; then
        echo "Usage: $0 <bw log dir> <report> <pattern> <rounds>"
        exit 1;
fi

DEBUG=0
USER="vsfgd"
#USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
LOGDIR=${1}
REPORT=${2}
PATTERN=${3}
ROUNDS=${4}

if [ $DEBUG = 1 ]; then
	set -x
fi

# header
echo -n "txseq," >> $REPORT
for i in `ls $LOGDIR/*.$PATTERN`; do
	host=`basename $i .$PATTERN`
	echo -n "$host," >> $REPORT
done
echo >> $REPORT

if [ -f $TMPFILE ] ; then
	rm $TMPFILE
fi

nseq=0
while [ $nseq -lt $ROUNDS ]; do
	echo $nseq >> $TMPFILE
	nseq=`expr $nseq + 1`
done

paste -d "," $TMPFILE `ls $LOGDIR/*.$PATTERN` >> $REPORT
