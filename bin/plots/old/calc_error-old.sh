#!/bin/bash

if [ $# -ne 4 ]; then
        echo "Usage: $0 <listdir> <allsiglog> <report> <rounds>"
        exit 1;
fi

DEBUG=1
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
TYPE="list"
LISTDIR=${1}
ALLSIGLOG=${2}
REPORT=${3}
ROUNDS=${4}
# scale for bc
SCL=6

if [ $DEBUG = 0 ]; then
	set -x
fi

nseq=0
echo "txseq, mean abs rel-error" >> $REPORT
while [ $nseq -lt $ROUNDS ] ; do
	var=`grep -m 1 "list T_0:" $LISTDIR/$TYPE.$nseq`
	ret=`echo $?`
	if [ $ret -ne 0 ]; then
		continue
	fi
	listsize=`echo $var | awk '{ print $4 }'`
	# skip dummy
	nsig=1
	absrelerror=0
	while [ $nsig -lt $listsize ]; do
		set -- `grep -m 1 "sig$nsig:" $LISTDIR/$TYPE.$nseq | awk '{ print $2, $6 }'`
		sig=$1
		# avg*p:$6
		avgp=$2

		# avg:$5
		trueavg=`grep -m 1 -e "^sig[0-9]*: $sig" $ALLSIGLOG | awk '{ print $5 }'`
		ret=`echo $?`
		if [ $ret -ne 0 ]; then
			echo "$sig not found in $ALLSIGLOG"
			exit 1;
		fi

		abserror=`echo "scale=$SCL; $avgp - $trueavg" | bc`
		# get absolute value
		abserror=`echo $abserror | awk ' { if($1>=0) { print $1 } else { print $1*-1 }}'`
		# convert awk's scientific notation to floating point
		abserror=`echo $abserror | awk -F"E" 'BEGIN{OFMT="%10.10f"} {print $1 * (10 ^ $2)}'`
		relerror=`echo "scale=$SCL; $abserror / $trueavg" | bc`
		
		absrelerror=`echo "scale=$SCL; $absrelerror + $relerror" | bc`
		
		nsig=`expr $nsig + 1`
	done
	meanabsrelerror=`echo "scale=$SCL; $absrelerror / $listsize" | bc`
	echo "$nseq, $meanabsrelerror" >> $REPORT
	nseq=`expr $nseq + 1`
done
