#!/bin/sh

if [ $# -ne 2 ]; then
	echo "Usage: $0 <sigdir> <initdir>"
	exit 1;
fi

USER=`whoami`
HOMEDIR="/home/$USER"
SIGDIR=$1
INITDIR=$2
GPSI="$HOMEDIR/build/chord/tools/gpsi"

# i is dir with sigs for 1 host
for i in `ls $SIGDIR` ; do
	$GPSI -r -s $SIGDIR/$i/ -P $INITDIR/log-$i.init
done
