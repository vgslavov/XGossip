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
echo "host,ver,listen intval,wait intval,broadcast intval,peers,seed,avg txlistlen,avg rxlistlen,recv bcast,recv bcastc,recv querys,recv queryx,recv queryxp,recv xgossip,recv xgossipc,recv vxgossip,sent xgossip(c)/vxgossip,recv informteam,sent informteam,recv initgossip,sent initgossip,total rxseq,total txseq,insert fail,insert succ,read failed,invalid msgtype,nothing recv,openfd,closefd,start ctime,start sincepoch,listen ctime,listen sincepoch,start exec ctime,start exec sincepoch,stop exec ctime,stop exec sincepoch,total sig read time,total init time,total exec time,total txmsglen" >> $REPORT

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

	# listen interval
	var=`grep -m 1 'listen interval:' $i`
	lintval=${var##*: }
	echo -n "$lintval," >> $REPORT

	# wait interval
	var=`grep -m 1 'wait interval:' $i`
	wintval=${var##*: }
	echo -n "$wintval," >> $REPORT

	# gossip interval
	var=`grep -m 1 'broadcast interval:' $i`
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

	# avg txlistlen
	avgtxlistlen=`grep 'txlistlen: ' $i | awk '{total=total+$4; n=n+1}END{print total/n}'`
        echo -n "$avgtxlistlen," >> $REPORT

	# avg rxlistlen
	avgrxlistlen=`grep 'rxlistlen: ' $i | awk '{total=total+$6; n=n+1}END{print total/n}'`
        echo -n "$avgrxlistlen," >> $REPORT

	# recv BCAST
	rxseq=`grep -c 'type: BCAST ' $i`
	echo -n "$rxseq," >> $REPORT

	# recv BCASTC
	rxseq=`grep -c 'type: BCASTC ' $i`
	echo -n "$rxseq," >> $REPORT

	# recv QUERYS
	rxseq=`grep -c 'type: QUERYS ' $i`
	echo -n "$rxseq," >> $REPORT

	# recv QUERYX
	rxseq=`grep -c 'type: QUERYX ' $i`
	echo -n "$rxseq," >> $REPORT

	# recv QUERYXP
	rxseq=`grep -c 'type: QUERYXP ' $i`
	echo -n "$rxseq," >> $REPORT

	# recv XGOSSIP
	rxseq=`grep -c 'type: XGOSSIP ' $i`
	echo -n "$rxseq," >> $REPORT

	# recv XGOSSIPC
	rxseq=`grep -c 'type: XGOSSIPC ' $i`
	echo -n "$rxseq," >> $REPORT

	# recv VXGOSSIP
	rxseq=`grep -c 'type: VXGOSSIP ' $i`
	echo -n "$rxseq," >> $REPORT

	# sent VXGOSSIP/XGOSSIP/XGOSSIPC
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

	# start exec time (ctime)
	var=`grep -m 1 'start exec ctime:' $i`
	#var=`grep -m 1 'start gossip ctime:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# start exec time (sincepoch)
	var=`grep -m 1 'start exec sincepoch:' $i`
	#var=`grep -m 1 'start gossip sincepoch:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# stop exec time (ctime)
	var=`grep -m 1 'stop exec ctime:' $i`
	#var=`grep -m 1 'stop gossip ctime:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# stop exec time (sincepoch)
	var=`grep -m 1 'stop exec sincepoch:' $i`
	#var=`grep -m 1 'stop gossip sincepoch:' $i`
	right=${var##*: }
	echo -n "$right," >> $REPORT

	# total sig read time
        var=`grep -m 1 'read time:' $i | awk '{print $3}'`
        echo -n "$var," >> $REPORT

	# init phase time
	var=`grep -m 1 'init phase time:' $i | awk '{print $5}'`
	echo -n "$var," >> $REPORT

	# exec phase time
	#var=`expr $gintval \* $txseq`
	#echo -n "$var," >> $REPORT
	var=`grep -m 1 'exec phase time:' $i | awk '{print $5}'`
	echo -n "$var," >> $REPORT

	# totaltxmsglen
	var=`grep -m 1 'totaltxmsglen:' $i | awk '{print $2}'`
	echo -n "$var," >> $REPORT

	echo >> $REPORT
done
