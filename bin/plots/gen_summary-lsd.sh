#!/bin/bash

if [ $# -ne 2 ]; then
        echo "Usage: $0 <lsd log dir> <report>"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
TYPE="lsd"
LOGDIR=${1}
REPORT=${2}
# TODO
#ROUNDS=${3}
RXSUM=0
TXSUM=0
# scale for bc
SCL=6

# header format:
echo "host,recv bcast,recv bcastc,recv queryxp,recv queryx,recv querys,recv vxgossip,recv vxgossipc,recv xgossip,recv xgossipc,recv initgossip,recv informteam,recv invalid,openfd,closefd,total recv,conn refused,too many open,no such file,total gsock,total succ recv" >> $REPORT

if [ $DEBUG = 1 ]; then
	set -x
fi

for i in `ls $LOGDIR/*.$TYPE` ; do
	# host
	var=`basename $i .$TYPE`
	var=`echo ${var#log\.}`
	#var=`hostname`
	echo -n "$var," >> $REPORT
	echo $var
	
	# recv bcast
	recv102=`grep -c 'msgtype: -23' $i`
	echo -n "$recv102," >> $REPORT

	# recv bcastc
	recv101=`grep -c 'msgtype: -24' $i`
	echo -n "$recv101," >> $REPORT

	# recv queryxp
	recv100=`grep -c 'msgtype: -21' $i`
	echo -n "$recv100," >> $REPORT

	# recv queryx
	recv0=`grep -c 'msgtype: -13' $i`
	echo -n "$recv0," >> $REPORT

	# recv querys
	recv1=`grep -c 'msgtype: -14' $i`
	echo -n "$recv1," >> $REPORT

	# recv vxgossip
	recv2=`grep -c 'msgtype: -15' $i`
	echo -n "$recv2," >> $REPORT

	# recv vxgossipc
	recv3=`grep -c 'msgtype: -22' $i`
	echo -n "$recv3," >> $REPORT

	# recv xgossip
	recv4=`grep -c 'msgtype: -18' $i`
	echo -n "$recv4," >> $REPORT

	# recv xgossipc
	recv5=`grep -c 'msgtype: -20' $i`
	echo -n "$recv5," >> $REPORT

	# recv initgossip
	recv6=`grep -c 'msgtype: -16' $i`
	echo -n "$recv6," >> $REPORT

	# recv informteam
	recv7=`grep -c 'msgtype: -17' $i`
	echo -n "$recv7," >> $REPORT

	# recv invalid
	recv8=`grep -c 'msgtype: -19' $i`
	echo -n "$recv8," >> $REPORT

	# opening socket
	openfd=`grep -c 'gossip: opening socket' $i`
	echo -n "$openfd," >> $REPORT

	# closing socket
	closefd=`grep -c 'gossip: closing socket' $i`
	echo -n "$closefd," >> $REPORT

	# total recv
	totalrecv=`expr $recv102 + $recv101 + $recv100 + $recv0 + $recv1 + $recv2 + $recv3 + $recv4 + $recv5 + $recv6 + $recv7 + $recv8`
	echo -n "$totalrecv," >> $REPORT

	# g-sock err1
	gsock1=`grep -c 'gossip: Error connecting to ./g-sock: Connection refused' $i`
	echo -n "$gsock1," >> $REPORT

	# g-sock err2
	gsock2=`grep -c 'gossip: Error connecting to ./g-sock: Too many open files' $i`
	echo -n "$gsock2," >> $REPORT

	# g-sock err3
	gsock3=`grep -c 'gossip: Error connecting to ./g-sock: No such file' $i`
	echo -n "$gsock3," >> $REPORT

	# total gsock
	totalgsock=`expr $gsock1 + $gsock2 + $gsock3`
	echo -n "$totalgsock," >> $REPORT

	# total succ recv
	totalsucc=`expr $totalrecv - $totalgsock`
	echo -n "$totalsucc," >> $REPORT

	echo >> $REPORT
done
