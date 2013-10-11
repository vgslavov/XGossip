#!/bin/bash

if [ $# -ne 3 ]; then
        echo "Usage: $0 <gpsi log> <report dir> <pattern>"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
LOGFILE=${1}
REPORTDIR=${2}
PATTERN=${3}
ROUNDS=${4}

if [ $DEBUG = 1 ]; then
	set -x
fi

HOST=`basename $LOGFILE .gpsi`
REPORT=$REPORTDIR/$HOST.$PATTERN
grep $PATTERN $LOGFILE > $REPORT
#grep -m $ROUNDS $PATTERN $LOGFILE | awk '{ print $2 }' > $REPORT
