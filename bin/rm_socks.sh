#!/bin/sh

if [ $# -ne 1 ]; then
        echo "Usage: rm_logs.sh <where>"
        exit 1;
fi

USER=vsfgd
HOME="/home/$USER"
WHERE=$1
PEERS=`ls $WHERE | wc -l`
TYPE="sock"

# 0-based
#PEERS=`expr $PEERS - 1`
while [ $PEERS -gt 0 ] ; do
	rm $WHERE/$PEERS/*-$TYPE;
	rm -rf $WHERE/$PEERS/db;
        PEERS=`expr $PEERS - 1`
done
