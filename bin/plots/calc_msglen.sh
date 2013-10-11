#!/bin/bash

if [ $# -ne 3 ]; then
        echo "Usage: $0 <msglen log dir> <report> <pattern>"
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
# scale for bc
SCL=6

if [ $DEBUG = 1 ]; then
	set -x
fi

if [ -f $TMPFILE ] ; then
	rm $TMPFILE
fi

echo "host,total msglen,avg msglen,# of msgs" >> $REPORT

for i in `ls $LOGDIR/*.$PATTERN` ; do
	host=`basename $i .$PATTERN`
	echo -n "$host," >> $REPORT
	sum=0
	avg=0
	nmsgs=`wc -l $i | awk '{print $1}'`
	while read line; do
		msglen=`echo $line`
		sum=`expr $sum + $msglen`
		avg=`echo "scale=$SCL; $sum / $nmsgs" | bc`
	done <"$i"
	echo "$sum,$avg,$nmsgs" >> $REPORT
done
