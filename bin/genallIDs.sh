#!/bin/sh

if [ $# -ne 2 ] ; then
	echo "Usage: $0 [dir with ID files] [allIDs file]"
	exit 1;
fi

USER=`whoami`
HOMEDIR="/home/$USER"
IDDIR=$1
OUTPUT=$2
TMPFILE="$HOMEDIR/`basename $0`.tmp"

if [ -f $TMPFILE ]; then
	rm $TMPFILE
fi

for i in `ls $IDDIR`; do
	awk '{print $4}' $IDDIR/$i | sort | uniq >> $TMPFILE
done

sort $TMPFILE | uniq > $OUTPUT
