#!/bin/sh

if [ $# -ne 4 ]; then
        echo "Usage: mv_logs_back.sh <peers> <type> <where> <mv|cp>"
        exit 1;
fi

USER=`whoami`
HOME="/home/$USER"
TMP="$HOME/tmp"
PEERS=${1}
TYPE=${2}
WHERE=${3}
ACTION=${4}
HOST=`hostname`
CP=0
MV=0

case "$ACTION" in
        (cp) CP=1;;
        (mv) MV=1;;
        (*) echo "not reached"; exit 127;;
esac


# 0-based
#PEERS=`expr $PEERS - 1`
while [ $PEERS -gt 0 ] ; do
	if [ $CP = 1 ] ; then
		cp $WHERE/$HOSTNAME-log-$PEERS.$TYPE $TMP/$PEERS/log.$TYPE;
	elif [ $MV = 1 ] ; then
		mv $WHERE/$HOSTNAME-log-$PEERS.$TYPE $TMP/$PEERS/log.$TYPE;
	fi
        PEERS=`expr $PEERS - 1`
done
