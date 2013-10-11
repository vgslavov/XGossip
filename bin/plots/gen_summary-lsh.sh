#!/bin/bash

if [ $# -ne 2 ]; then
        echo "Usage: $0 <logdir> <report>"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
TYPE="lsh"
LOGDIR=${1}
REPORT=${2}
# TODO
#ROUNDS=${3}
RXSUM=0
TXSUM=0
# scale for bc
SCL=6

# header format:
echo "host,mgroups,lfuncs,siglist size,uniqueids,groupedT.size()" >> $REPORT

if [ $DEBUG = 1 ]; then
	set -x
fi

for i in `ls $LOGDIR/*.$TYPE` ; do
	# host
	var=`basename $i .$TYPE`
	var=`echo ${var#log\.}`
	#var=`hostname`
	echo -n "$var," >> $REPORT

	# bands
	var=`grep mgroups $i | awk '{print $2}'`
	echo -n "$var," >> $REPORT

	# rows
	var=`grep lfuncs $i | awk '{print $2}'`
	echo -n "$var," >> $REPORT

	# siglist size
	var=`grep 'readsig: ' $i | tail -1 | awk '{print $6}'`
	#var=`grep 'splitfreq: setsize: ' $i | awk '{print $3}'`
	echo -n "$var," >> $REPORT

	# set size
	var=`grep 'uniqueids: ' $i | awk '{print $2}'`
	echo -n "$var," >> $REPORT

	# groupedT.size()
	var=`grep 'groupedT.size(): ' $i | awk '{print $2}'`
	echo -n "$var," >> $REPORT

	echo >> $REPORT
done
