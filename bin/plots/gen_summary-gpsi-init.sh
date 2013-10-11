#!/bin/bash

if [ $# -ne 2 ]; then
        echo "Usage: $0 <gpsi log dir> <report>"
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
echo "host,ver,intval b/w inserts,listen intval,wait intval,gossip intval,peers,seed,mgroups,lfuncs,b:sumavg(S(f/w)),b:multisetsize(S(f/w * peers)),b:setsize(unique),a:sumavg(S(f/w)),a:multisetsize(S(f/w * peers)),a:setsize(unique),init disc,exec disc,avg lists merged,avg merge time,avg txlistlen,avg rxlistlen,recv xgossip,recv xgossip,sent vxgossip,recv informteam,sent informteam,recv initgossip,sent initgossip,total rxseq,total txseq,insert fail,insert succ,read failed,invalid msgtype,nothing recv,openfd,closefd,start ctime,start sincepoch,listen ctime,listen sincepoch,total sig read time,total init time," >> $REPORT

if [ $DEBUG = 1 ]; then
	set -x
fi

for i in `ls $LOGDIR/*.$TYPE` ; do
#for i in `ls $LOGDIR/*/*.$TYPE` ; do
	# host
        var=`basename $i .$TYPE`
        #var=`echo ${var#log\.}`
	#var=`grep -m 1 'host:' $i|awk '{print $2}'`
	echo -n "$var," >> $REPORT
	echo $var

	# version
	var=`grep -m 1 'rcsid:' $i|awk '{print $4}'`
	echo -n "$var," >> $REPORT

	# interval b/w inserts
        var=`grep -m 1 'interval b/w inserts:' $i`
        iintval=${var##*: }
        echo -n "$iintval," >> $REPORT

        # listen interval
        var=`grep -m 1 'listen interval:' $i`
        lintval=${var##*: }
        echo -n "$lintval," >> $REPORT

        # wait interval
        var=`grep -m 1 'wait interval:' $i`
        wintval=${var##*: }
        echo -n "$wintval," >> $REPORT
	
	# gossip interval
	var=`grep -m 1 'gossip interval:' $i`
	gintval=${var##*: }
	echo -n "$gintval," >> $REPORT

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

	# before init/exec phase (1st printlist)
	set -- `grep -m 1 'printlist:' $i | awk '{print $3, $5, $7}'`
	bsumavg=$1
	bmultisetsize=$2
	bsetsize=$3
	echo -n "$bsumavg," >> $REPORT
	echo -n "$bmultisetsize," >> $REPORT
	echo -n "$bsetsize," >> $REPORT

	# after init/exec phase sigs (last printlist)
	set -- `grep 'printlist:' $i | tail -1 | awk '{print $3, $5, $7}'`
	asumavg=$1
	amultisetsize=$2
	asetsize=$3
	echo -n "$asumavg," >> $REPORT
	echo -n "$amultisetsize," >> $REPORT
	echo -n "$asetsize," >> $REPORT

	# older seq
	#discold=`grep -c 'rxseq<txseq' $i`
	#echo -n "$discold," >> $REPORT

	# newer seq
	#discnew=`grep -c 'rxseq>txseq' $i`
	#echo -n "$discnew," >> $REPORT

	# older + newer seq
	#totalout=`expr $discold + $discnew`
	#echo -n "$totalout," >> $REPORT

	# init discarded
	totaldisc=`grep -c 'warning: phase=init' $i`
	echo -n "$totaldisc," >> $REPORT

	# exec discarded
	totaldisc=`grep -c 'warning: phase=exec' $i`
	echo -n "$totaldisc," >> $REPORT

	# avg seq diff
	#avgdiff=`grep 'warning: rxseq' $i | awk '{total=total+$4; n=n+1}END{print total/n}'`
	#echo -n "$avgdiff," >> $REPORT

	# avg lists merged
	avgmerged=`grep 'initial allT.size():' $i | awk '{total=total+$3; n=n+1}END{print total/n}'`
	echo -n "$avgmerged," >> $REPORT

	# avg merge time
	tmp=`grep 'merge lists time:' $i | awk '{total=total+$4; n=n+1}END{print total/n}'`
	avgmergetime=`echo $tmp | awk -F"E" 'BEGIN{OFMT="%10.10f"} {print $1 * (10 ^ $2)}'`
	echo -n "$avgmergetime," >> $REPORT

	# avg txlistlen
	avgtxlistlen=`grep 'txlistlen: ' $i | awk '{total=total+$4; n=n+1}END{print total/n}'`
        echo -n "$avgtxlistlen," >> $REPORT

	# avg rxlistlen
	avgrxlistlen=`grep 'rxlistlen: ' $i | awk '{total=total+$6; n=n+1}END{print total/n}'`
        echo -n "$avgrxlistlen," >> $REPORT

	# recv XGOSSIP
	rxseq=`grep -c 'type: XGOSSIP ' $i`
	echo -n "$rxseq," >> $REPORT

	# recv VXGOSSIP
	rxseq=`grep -c 'type: VXGOSSIP ' $i`
	echo -n "$rxseq," >> $REPORT

	# sent XGOSSIP/XGOSSIPP
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

	# total rxseq
	rxseq=`grep -c 'rxseq:' $i`
	echo -n "$rxseq," >> $REPORT

	# total txseq
	txseq=`grep -c 'txseq:' $i`
	echo -n "$txseq," >> $REPORT

	# insert fail
	insfail=`grep -c 'insert FAILed' $i`
	echo -n "$insfail," >> $REPORT

	# insert succ
	inssucc=`grep -c 'insert SUCCeeded' $i`
	echo -n "$inssucc," >> $REPORT

	# read failed
	err2=`grep -c 'readgossip: read failed' $i`
	echo -n "$err2," >> $REPORT

	# invalid msgtype
	err3=`grep -c 'error: invalid msgtype' $i`
	echo -n "$err3," >> $REPORT

	# nothing received
	err4=`grep -c 'readgossip: nothing received' $i`
	echo -n "$err4," >> $REPORT

	# openfd
	var=`grep 'openfd:' $i | awk '{print $2}'`
	echo -n "$var," >> $REPORT

	# closefd
	var=`grep 'closefd:' $i | awk '{print $2}'`
	echo -n "$var," >> $REPORT

	# start time (ctime)
        var=`grep -m 1 'start ctime:' $i`
        right=${var##*: }
        echo -n "$right," >> $REPORT

        # start time (sincepoch)
        var=`grep -m 1 'start sincepoch:' $i`
        right=${var##*: }
        echo -n "$right," >> $REPORT

        # listen time (ctime)
        var=`grep -m 1 'listen ctime:' $i`
        right=${var##*: }
        echo -n "$right," >> $REPORT

        # listen time (sincepoch)
        var=`grep -m 1 'listen sincepoch:' $i`
        right=${var##*: }
        echo -n "$right," >> $REPORT

        # total sig read time
        var=`grep -m 1 'read time:' $i | awk '{print $3}'`
        echo -n "$var," >> $REPORT

        # total init phase time
        var=`grep -m 1 'init phase time:' $i | awk '{print $5}'`
        echo -n "$var," >> $REPORT

	echo >> $REPORT
done
