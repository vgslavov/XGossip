#!/bin/bash

if [ $# -ne 3 ]; then
        echo "Usage: $0 <gpsi log dir> <report dir> <pattern>"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
LOGDIR=${1}
REPORTDIR=${2}
PATTERN=${3}
EXTRACT="$HOMEDIR/bin/plots/extract_rxpattern.sh"

if [ $DEBUG = 1 ]; then
	set -x
fi

for i in `ls $LOGDIR` ; do
	HOST=`basename $i .gpsi`
        echo $HOST
	$EXTRACT $LOGDIR/$i $REPORTDIR $PATTERN
done
