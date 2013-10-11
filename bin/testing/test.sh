#!/bin/bash

TMPFILE="/home/vsfgd/tmp/test.tmp"
highseq=137

#set -x

# total rxbw
grep -n 'rxmsglen' $1 > $TMPFILE
exec 3<&0
exec 0<"$TMPFILE"
RXSUM=0
z=0
linenum=`grep -m 1 -n "txlistlen: $highseq" $1 | awk -F: '{print $1}'`
while read line; do
	right=${line##*: }
	if [ $z -eq 0 ] ; then
		var=`echo -n $line | awk -F: '{print $1}'`
	fi
	# rxbw2high
	if [ $var -gt $linenum ] ; then
		echo -n "$RXSUM, "
		z=1
		var=0
	fi
	RXSUM=`expr $RXSUM + $right`
done <"$TMPFILE"                        # loop reads from tmpfile
exec 0<&3
exec 3<&-
echo -n "$RXSUM, "

