#!/bin/bash

if [ $# -ne 2 ]; then
	echo "Usage: $0 <query teamIDs> <bad teamIDs>"
	exit 1;
fi

QUERYIDS=${1}
BADIDS=${2}
NQUERYIDS=`wc -l $QUERYIDS`
NBADIDS=`wc -l $BADIDS`

n=0
for i in `cat $QUERYIDS`; do
	ret=`grep -c $i $BADIDS`
	if [ $ret -ne 0 ] ; then
		#echo teamID: $i
		n=`expr $n + 1`
	fi
done

echo Query teamIDs: $NQUERYIDS
echo Bad teamIDs: $NBADIDS
echo Found $n query teamIDs in bad teamIDs
