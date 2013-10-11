#!/bin/bash

if [ $# -ne 2 ]; then
        echo "Usage: $0 <logdir> <report>"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
TYPE="gpsi"
LOGDIR=${1}
REPORT=${2}
# TODO
#ROUNDS=${3}
RXSUM=0
TXSUM=0
# scale for bc
SCL=6

# header format:
#host,ver,interval,peers,seed,older seq,newer seq,older+newer,total disc,avg seq diff,avg lists merged,avg merge time,total rxseq,total txseq,insert fail,insert succ,start time,total time,unique sigs,txseq2high,time2high,rxbw2high,total rxbw,txbw2high,total txbw

if [ $DEBUG = 1 ]; then
	set -x
fi

for i in `ls $LOGDIR/*.$TYPE` ; do
	# host
	var=`grep -m 1 'host:' $i`
	# everything to the right of the rightmost colon
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# version
	var=`grep -m 1 'rcsid:' $i|awk '{print $4}'`
	echo -n "$var," >> $REPORT

	# gossip interval
	var=`grep -m 1 'interval:' $i`
	intval=${var##*: }
	echo -n "$intval," >> $REPORT

	# peers
	var=`grep -m 1 'peers:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# local seed
	var=`grep -m 1 'loseed:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# older seq
	discold=`grep -c 'rxseq<txseq' $i`
	echo -n "$discold," >> $REPORT

	# newer seq
	discnew=`grep -c 'rxseq>txseq' $i`
	echo -n "$discnew," >> $REPORT

	# older + newer seq
	totalout=`expr $discold + $discnew`
	echo -n "$totalout," >> $REPORT

	# total discarded
	totaldisc=`grep -c 'msg discarded' $i`
	echo -n "$totaldisc," >> $REPORT

	# avg seq diff
	avgdiff=`grep 'warning: rxseq' $i | awk '{total=total+$4; n=n+1}END{print total/n}'`
	echo -n "$avgdiff," >> $REPORT

	# avg lists merged
	avgmerged=`grep 'add2vecomap:' $i | awk '{total=total+$3; n=n+1}END{print total/n}'`
	echo -n "$avgmerged," >> $REPORT

	# avg merge time
	tmp=`grep 'merge lists time:' $i | awk '{total=total+$4; n=n+1}END{print total/n}'`
	avgmergetime=`echo $tmp | awk -F"E" 'BEGIN{OFMT="%10.10f"} {print $1 * (10 ^ $2)}'`
	echo -n "$avgmergetime," >> $REPORT

	# total rxseq
	rxseq=`grep -c 'rxseq:' $i`
	echo -n "$rxseq," >> $REPORT

	#ratiorxdisc=`echo "scale=$SCL; $totaldisc / $rxseq" | bc`
	#echo -n "$ratiorxdisc," >> $REPORT

	# total txseq
	txseq=`grep -c 'txseq:' $i`
	echo -n "$txseq," >> $REPORT

	# insert fail
	insfail=`grep -c 'insert FAILed' $i`
	echo -n "$insfail," >> $REPORT

	# insert succ
	inssucc=`grep -c 'insert SUCCeeded' $i`
	echo -n "$inssucc," >> $REPORT

	# start time
	var=`grep -m 1 'sincepoch:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# total time
	var=`expr $intval \* $txseq`
	echo -n "$var," >> $REPORT

	# unique sigs
	highlistlen=`grep 'txlistlen:' $i | tail -1 | awk '{print $4}'`
	echo -n "$highlistlen," >> $REPORT

	# txseq2high: at which round that ocurred for the first time
	grep 'txlistlen:' $i |awk '{print $4}' > $TMPFILE
	# save stdin to fd 3
	exec 3<&0
	# redirect stdin
	exec 0<"$TMPFILE"
	highseq=0
	while read line; do
		# everything to the right of the rightmost colon
		right=${line##*: }
		highseq=`expr $highseq + 1`
		if [ $right -eq $highlistlen ] ; then
			break
		fi
	done <"$TMPFILE"
	# restore old stdin
	exec 0<&3
	# close temp fd 3
	exec 3<&-
	echo -n "$highseq," >> $REPORT 

	# time2high
	var=`expr $intval \* $highseq`
	echo -n "$var," >> $REPORT

	# total rxbw
	grep -n 'rxmsglen:' $i > $TMPFILE
	exec 3<&0
	exec 0<"$TMPFILE"
	RXSUM=0
	z=0

	linenum=`grep -m 1 -n "txlistlen: $highlistlen" $i | awk -F: '{print $1}'`
	while read line; do
		msglen=`echo $line | awk '{print $2}'`
		if [ $z -eq 0 ] ; then
			var=`echo -n $line | awk -F: '{print $1}'`
		fi
		# rxbw2high
		if [ $var -gt $linenum ] ; then
			echo -n "$RXSUM," >> $REPORT
			z=1
			var=0
		fi
		RXSUM=`expr $RXSUM + $msglen`
	done <"$TMPFILE"			# loop reads from tmpfile
	exec 0<&3
	exec 3<&-
	echo -n "$RXSUM," >> $REPORT

	# total txbw
	grep 'txmsglen:' $i > $TMPFILE
	exec 3<&0
	exec 0<"$TMPFILE"
	TXSUM=0
	n=0
	while read line; do
		msglen=`echo $line | awk '{print $2}'`
		TXSUM=`expr $TXSUM + $msglen`
		n=`expr $n + 1`
		# txbw2high
		if [ $n -eq $highseq ] ; then
			echo -n "$TXSUM," >> $REPORT
		fi
	done <"$TMPFILE"
	exec 0<&3
	exec 3<&-
	echo "$TXSUM" >> $REPORT
done
