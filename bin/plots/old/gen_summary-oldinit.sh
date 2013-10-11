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
#host,ver,init interval,wait interval,gossip interval,init phase time,peers,seed,mgroups,lfuncs,total sigs (before init),unique sigs (before init),unique sigs (after init),unique isigs (after init),sigs+isigs (after init),older seq,newer seq,older+newer,init disc,exec disc,avg seq diff,avg lists merged,avg merge time,recv xgossipp,sent xgossipp,recv informteam,sent informteam,recv initgossip,sent initgossip,insert fail,insert succ,init start time,exec start time

if [ $DEBUG = 1 ]; then
	set -x
fi

for i in `ls $LOGDIR/*.$TYPE` ; do
	# host
	#var=`grep -m 1 'host:' $i`
	# everything to the right of the rightmost colon
	#right=${var##*: }
	#echo -n "$right," >> $REPORT

	# host
	var=`grep -m 1 'host:' $i|awk '{print $2}'`
	echo -n "$var," >> $REPORT

	# version
	var=`grep -m 1 'rcsid:' $i|awk '{print $4}'`
	echo -n "$var," >> $REPORT

	# init interval
	var=`grep -m 1 'init interval:' $i`
	iintval=${var##*: }
	echo -n "$iintval," >> $REPORT

	# wait interval
	var=`grep -m 1 'wait interval:' $i`
	wintval=${var##*: }
	echo -n "$wintval," >> $REPORT

	# gossip interval
	var=`grep -m 1 'gossip interval:' $i`
	gintval=${var##*: }
	echo -n "$gintval," >> $REPORT

	# init phase time
	var=`grep -m 1 'init phase time:' $i | awk '{print $5}'`
	echo -n "$var," >> $REPORT

	# peers
	var=`grep -m 1 'peers:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# local seed
	var=`grep -m 1 'loseed:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# mgroups
	var=`grep -m 1 'mgroups:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# lfuncs
	var=`grep -m 1 'lfuncs:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# before init phase total & unique sigs
	set -- `grep -m 1 'printlist:' $i | awk '{print $3, $7}'`
	totalsigs=$1
	uniquesigs=$2
	echo -n "$totalsigs," >> $REPORT
	echo -n "$uniquesigs," >> $REPORT

	# after init phase sigs
	initsig=`grep 'printlist:' $i | tail -1 | awk '{print $9}'`
	echo -n "$initsig," >> $REPORT

	# after init phase isigs
	initisig=`grep 'printlist:' $i | tail -1 | awk '{print $11}'`
	echo -n "$initisig," >> $REPORT

	# after init phase total sigs
	atotalsigs=`expr $initsig + $initisig`
	echo -n "$atotalsigs," >> $REPORT

	# older seq
	discold=`grep -c 'rxseq<txseq' $i`
	echo -n "$discold," >> $REPORT

	# newer seq
	discnew=`grep -c 'rxseq>txseq' $i`
	echo -n "$discnew," >> $REPORT

	# older + newer seq
	totalout=`expr $discold + $discnew`
	echo -n "$totalout," >> $REPORT

	# init discarded
	totaldisc=`grep -c 'warning: phase=init' $i`
	echo -n "$totaldisc," >> $REPORT

	# exec discarded
	totaldisc=`grep -c 'warning: phase=exec' $i`
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

	# recv XGOSSIPP
	rxseq=`grep -c 'type: XGOSSIPP' $i`
	echo -n "$rxseq," >> $REPORT

	# sent XGOSSIPP
	rxseq=`grep -c 'txID:' $i`
	echo -n "$rxseq," >> $REPORT

	# recv INFORMTEAM
	rxseq=`grep -c 'type: INFORMTEAM' $i`
	echo -n "$rxseq," >> $REPORT

	# sent INFORMTEAM
	rxseq=`grep -c 'inserting INFORMTEAM:' $i`
	echo -n "$rxseq," >> $REPORT

	# recv INITGOSSIP
	rxseq=`grep -c 'type: INITGOSSIP' $i`
	echo -n "$rxseq," >> $REPORT

	# sent INITGOSSIP
	rxseq=`grep -c 'inserting -16:' $i`
	echo -n "$rxseq," >> $REPORT

	#ratiorxdisc=`echo "scale=$SCL; $totaldisc / $rxseq" | bc`
	#echo -n "$ratiorxdisc," >> $REPORT

	# total txseq
	#txseq=`grep -c 'txseq:' $i`
	#echo -n "$txseq," >> $REPORT

	# insert fail
	insfail=`grep -c 'insert FAILed' $i`
	echo -n "$insfail," >> $REPORT

	# insert succ
	inssucc=`grep -c 'insert SUCCeeded' $i`
	echo -n "$inssucc," >> $REPORT

	# init start time
	var=`grep -m 1 'sincepoch:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# exec start time
	var=`grep -m 1 'execsincepoch:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	echo >> $REPORT

	# total time
	#var=`expr $intval \* $txseq`
	#echo -n "$var," >> $REPORT

	# unique sigs
	#highlistlen=`grep 'txlistlen:' $i | tail -1 | awk '{print $4}'`
	#echo -n "$highlistlen," >> $REPORT
done
