#!/bin/bash

if [ $# -ne 2 ]; then
        echo "Usage: $0 <gpsi log dir> [report]"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
LOGDIR=${1}
REPORT=${2}
RXPATTERN="rxmsglen"
TXPATTERN="roundtxmsglen"

if [ $DEBUG = 1 ]; then
	set -x
fi

echo "host,rx total,tx total" >> $REPORT

for i in `ls $LOGDIR` ; do
	HOST=`basename $i .gpsi`
        echo -n "$HOST," >> $REPORT
	rxtotal=`grep $RXPATTERN $LOGDIR/$i | awk '{total=total + $2}END{print total}'`
	echo -n "$rxtotal," >> $REPORT
	txtotal=`grep $TXPATTERN $LOGDIR/$i | awk '{total=total + $2}END{print total}'`
	echo -n "$txtotal," >> $REPORT
	echo >> $REPORT
done
