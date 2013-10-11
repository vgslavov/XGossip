#!/bin/bash

if [ $# -ne 3 ]; then
	echo "Usage: $0 <results dir> <team log> <output file>"
	exit 1;
fi

RESULTSDIR=${1}
TEAMLOG=${2}
OUTPUTFILE=${3}

n=0
for i in `ls $RESULTSDIR`; do
	teamID=`grep -m 1 "teamID: " $RESULTSDIR/$i | awk '{print $8}'`
	ret=`grep -c $teamID $TEAMLOG`	
	if [ $ret -eq 0 ] ; then
		n=`expr $n + 1`
		echo $n excluded
		echo $i >> $OUTPUTFILE
	fi
done
