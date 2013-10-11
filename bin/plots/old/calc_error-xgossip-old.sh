#!/bin/bash

if [ $# -ne 5 ]; then
        echo "Usage: $0 <listdir> <allsiglog> <report> <rounds> <minfreq>"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
TYPE="list"
LISTDIR=${1}
ALLSIGLOG=${2}
REPORT=${3}
ROUNDS=${4}
MINFREQ=${5}
# scale for bc
SCL=6

if [ $DEBUG = 1 ]; then
	set -x
fi

nseq=-1
lowfreqsigs=0
notfoundsigs=0
totalsigs=0
echo "txseq, mean abs rel-error, not found sigs, low freq sigs, regular sigs, multisets, total sigs" >> $REPORT
while [ $nseq -lt $ROUNDS ] ; do
	var=`grep -m 1 "list T_0:" $LISTDIR/$TYPE.$nseq`
	set -- `grep -m 1 "printlist: sumavg: " $LISTDIR/$TYPE.$nseq | awk '{print $9, $11}'`
	regularsigs=$1
	multisets=$2

	ret=`echo $?`
	if [ $ret -ne 0 ]; then
		continue
	fi
	listsize=`echo $var | awk '{ print $4 }'`
	# don't skip dummy in XGossip+
	nsig=0
	absrelerror=0
	while [ $nsig -lt $listsize ]; do
		set -- `grep -m 1 "sig$nsig:" $LISTDIR/$TYPE.$nseq | awk '{ print $2, $7 }'`
		sig=$1
		# avg*n:$6
		# avg*q:$7
		avgp=$2

		# avg:$5
		trueavg=`grep -m 1 -e "^sig[0-9]*: $sig" $ALLSIGLOG | awk '{ print $5 }'`
		#ret=`echo $?`
		#if [ $ret -ne 0 ]; then

		# skip sig if not found (special sig)
		if [ -z "$trueavg" ]; then
			#echo "$sig not found in $ALLSIGLOG"
			notfoundsigs=`expr $notfoundsigs + 1`
			totalsigs=`expr $totalsigs + 1`
			nsig=`expr $nsig + 1`
			continue
			#exit 1;
		fi

		abserror=`echo "scale=$SCL; $avgp - $trueavg" | bc`
		# get absolute value
		abserror=`echo $abserror | awk ' { if($1>=0) { print $1 } else { print $1*-1 }}'`
		# convert awk's scientific notation to floating point
		abserror=`echo $abserror | awk -F"E" 'BEGIN{OFMT="%10.10f"} {print $1 * (10 ^ $2)}'`
		relerror=`echo "scale=$SCL; $abserror / $trueavg" | bc`
		
		absrelerror=`echo "scale=$SCL; $absrelerror + $relerror" | bc`
		
		totalsigs=`expr $totalsigs + 1`
		nsig=`expr $nsig + 1`
	done
	meanabsrelerror=`echo "scale=$SCL; $absrelerror / $listsize" | bc`
echo "$nseq, $meanabsrelerror, $notfoundsigs, $lowfreqsigs, $regularsigs, $multisets, $totalsigs" >> $REPORT
	nseq=`expr $nseq + 1`
	totalsigs=0
	notfoundsigs=0
done
