#!/bin/sh

if [ $# -ne 2 ]; then
        echo "Usage: rm_logs.sh <peers> <file>"
        exit 1;
fi

USER=vsfgd
HOME="/home/$USER"
TMP="$HOME/tmp"
PEERS=${1}
FILE=${2}

# 0-based
PEERS=`expr $PEERS - 1`
while [ $PEERS -ge 0 ] ; do
        rm $TMP/$PEERS/$FILE;
        PEERS=`expr $PEERS - 1`
done
