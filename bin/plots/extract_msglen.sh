#!/bin/bash

if [ $# -ne 2 ]; then
        echo "Usage: $0 <gpsi log> <report dir>"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
LOGFILE=${1}
REPORTDIR=${2}
RXPATTERN="rxmsglen"
TXPATTERN="txmsglen"

if [ $DEBUG = 1 ]; then
	set -x
fi

HOST=`basename $LOGFILE .gpsi`
RXREPORT=$REPORTDIR/$HOST.$RXPATTERN
TXREPORT=$REPORTDIR/$HOST.$TXPATTERN
#grep -m $ROUNDS $PATTERN $LOGFILE | awk '{print $2}' > $REPORT
#grep -m $ROUNDS $PATTERN $LOGFILE | awk '{print $2}' | awk -F "," '{print $1}' > $REPORT
grep -e ^${RXPATTERN}: $LOGFILE | awk '{print $2}' > $RXREPORT
grep -e ^${TXPATTERN}: $LOGFILE | awk '{print $2}' > $TXREPORT
