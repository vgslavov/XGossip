#!/bin/bash

if [ $# -ne 4 ]; then
        echo "Usage: $0 <bw log dir> <report> <rxpattern> <rounds>"
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

# rounds column
nseq=0
while [ $nseq -lt $ROUNDS ]; do
	echo $nseq >> $TMPFILE
	nseq=`expr $nseq + 1`
done

# combine same-round rxmsglens for each peer
for i in `ls $LOGDIR/*.$PATTERN` ; do
	host=`basename $i .$PATTERN`
	RXSUM=0
	PREVTXSEQ=0
	z=0
	nseq=0
	while read line; do
		set -- `echo $line | awk '{ print $2, $4, $8, $10 }'`
		rxmsglen=$1
		msgtype=$2
		rxseq=$3
		txseq=$4

		# skip queries
		if [ $msgtype = "QUERYX" -o $msgtype = "QUERYXP" ]; then
			continue
		fi

		# find lowest seq
		if [ $z -eq 0 ] ; then
			PREVTXSEQ=$txseq
			z=1
		fi
		if [ $txseq -eq $PREVTXSEQ ] ; then
			RXSUM=`expr $RXSUM + $rxmsglen`
			PREVRXSUM=$RXSUM
		else
			# pad seq
			while [ $PREVTXSEQ -gt $nseq ] ; do
				echo "0" >> $LOGDIR/$host.$PATTERN.new
				nseq=`expr $nseq + 1`
			done
			# write only after you are at the next round
			# but before you update the sum
			echo "$PREVRXSUM" >> $LOGDIR/$host.$PATTERN.new
			RXSUM=$rxmsglen
			PREVRXSUM=$RXSUM
			PREVTXSEQ=$txseq
			nseq=`expr $nseq + 1`
		fi
	done <"$i"

	# if results file was empty!
	if [ $z -eq 0 ] ; then
		# don't skip this peer!
		echo "0" >> $LOGDIR/$host.$PATTERN.new
		continue
	fi

	# last rxmsglen (line) hasn't been recorded yet!
	while [ $PREVTXSEQ -gt $nseq ] ; do
		echo "0" >> $LOGDIR/$host.$PATTERN.new
		nseq=`expr $nseq + 1`
	done
	echo "$PREVRXSUM" >> $LOGDIR/$host.$PATTERN.new
done

# data
paste -d "," $TMPFILE `ls $LOGDIR/*.$PATTERN.new` >> $REPORT
