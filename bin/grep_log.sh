#!/bin/sh

if [ $# -ne 3 ]; then
        echo "Usage: grep_log.sh <type> <lines> <pattern>"
        exit 1;
fi

USER=vsfgd
HOME="/home/$USER"
TMP="$HOME/tmp"
TYPE=${1}
LINES=${2}
PATTERN=${3}

for i in `ls *.$TYPE` ; do echo $i:; cat $i |grep -B $LINES "$PATTERN"; done
