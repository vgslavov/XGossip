#!/bin/sh

if [ $# -ne 5 ]; then
        echo "Usage: cp_logs.sh <peers> <type> <mv|cp> <src> <dst>"
        exit 1;
fi

USER=`whoami`
HOME="/home/$USER"
TMP="$HOME/tmp"
PEERS=${1}
TYPE=${2}
ACTION=${3}
SRC=${4}
DST=${5}
HOST=`hostname`
CP=0
MV=0

if [ $ACTION != "mv" -a $ACTION != "cp" ] ; then
        echo "Usage: cp_logs.sh <peers> <type> <mv|cp> <src> <dst>"
	exit 1;
fi

# 0-based
#PEERS=`expr $PEERS - 1`
while [ $PEERS -gt 0 ] ; do
	$ACTION $SRC/$PEERS/log.$TYPE $DST/$HOSTNAME-log-$PEERS.$TYPE;
        PEERS=`expr $PEERS - 1`
done
