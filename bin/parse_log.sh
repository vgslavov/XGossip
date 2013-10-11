#!/bin/sh

USER=vsfgd
HOMEDIR="/home/$USER"
LOGDIR="$HOMEDIR/log"

for i in `ls $LOGDIR/$1/$2/*.$1` ; do
	echo "$i"
	cat $i |grep "$3" "$4"
done
