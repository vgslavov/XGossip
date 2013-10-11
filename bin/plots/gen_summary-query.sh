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
# scale for bc
SCL=6

# header format:
#echo "host,QUERYS,found,not found," >> $REPORT

if [ $DEBUG = 1 ]; then
	set -x
fi

for i in `ls $LOGDIR/*.$TYPE` ; do
	# host
	#var=`basename $i .$TYPE`
	#var=`echo ${var#log\.}`
	var=`hostname`
	echo -n "$var," >> $REPORT

	# total queries
	var=`grep -c QUERYS $i`
	echo -n "$var," >> $REPORT

	# found
	var=`grep -c "query result:  found" $i`
	echo -n "$var," >> $REPORT

	# not found
	var=`grep -c "query result:  NOT found" $i`
	echo -n "$var," >> $REPORT

	echo >> $REPORT
done
