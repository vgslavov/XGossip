#!/bin/bash

if [ $# -ne 9 ]; then
        echo "Usage: $0 [lists dir] [all sigs log] [report] [teamID-NOT-found log] [all teamIDs] <last round> <min freq> <team size> <# of instances>"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
#TYPE="list"
LISTDIR=${1}
ALLSIGLOG=${2}
REPORT=${3}
TEAMIDNOTFOUNDLOG=${4}
ALLTEAMIDSLOG=${5}
ROUNDS=${6}
MINFREQ=${7}
# TODO: use printlist value
TEAMSIZE=${8}
# the same dataset was used on each of the N nodes in the cluster
NDATASETS=${9}
# scale for bc
SCL=6

if [ $DEBUG = 1 ]; then
	set -x
fi

# get a list of teamIDs
TEAMIDS=`ls $LISTDIR | awk -F . '{print $1}' | uniq`
# TODO: find a better way
NTEAMIDS=`ls $LISTDIR | awk -F . '{print $1}' | uniq | wc -l`
echo "NTEAMIDS: " $NTEAMIDS

nseq=$ROUNDS
lowfreqsigs=0
notfoundsigs=0
totalsigs=0
tidskipped=0

echo "teamID,found?,rel. error (%),freq (<=$MINFREQ),sigs" >> $REPORT

echo "seq: " $nseq

n=1
for tid in $TEAMIDS ; do
	echo "teamID ($n/$NTEAMIDS): " $tid

	echo -n "$tid," >> $REPORT

	# TODO: remove TEAMIDSNOTFOUND in advance
	ret=`grep -c $tid $TEAMIDNOTFOUNDLOG`
	if [ $ret -ne 0 ] ; then
		tidskipped=`expr $tidskipped + 1`
		echo -n "n," >> $REPORT
	else
		echo -n "y," >> $REPORT
	fi

	listhdr=`grep -m 1 "list T_0:" $LISTDIR/$tid.$nseq`
	regularsigs=0
	# xgossip
	set -- `grep -m 1 "printlist: sumavg: " $LISTDIR/$tid.$nseq | awk '{print $7, $13}'`
	regularsigs=$1
	multisets=$2

	# TODO: needed?
	if [ -z "$regularsigs" ]; then
		continue
	fi

	listsize=`echo $listhdr | awk '{ print $8 }'`
	# skip 1 dummy in vanilla
	#nsig=1
	# skip 2 dummies in xgossip
	nsig=2
	absrelerror=0
	while [ $nsig -lt $listsize ]; do
		# avg:$5
		# avg*peers:$6
		# avg*teamsize:$7

		# xgossip
		# use 'avg' until printlist is fixed in new version
		set -- `grep -m 1 "sig$nsig:" $LISTDIR/$tid.$nseq | awk '{ print $2, $5 }'`
		#set -- `grep -m 1 "sig$nsig:" $LISTDIR/$tid.$nseq | awk '{ print $2, $7 }'`
		# vanilla
		# use 'avg*peers'
		#set -- `grep -m 1 "sig$nsig:" $LISTDIR/$tid.$nseq | awk '{ print $2, $6 }'`
		sig=$1
		# multiply 'avg' times teamsize until printlist is fixed in new version
		avgp=`echo "scale=$SCL; $2 * $TEAMSIZE" | bc`

		# trueavg:$5
		# truefreq:$3
		# trueavg == truefreq (in ALLSIGLOG)
		# space after sig is NEEDED!
		trueavg=`grep -m 1 -e "^sig[0-9]*: $sig " $ALLSIGLOG | awk '{ print $5 }'`
		# the same dataset was used on each of the $NDATASETS nodes in the cluster
		trueavg=`expr $trueavg \* $NDATASETS`

		# 1st: check if sig is found
		if [ -z "$trueavg" ]; then
			#echo "$sig not found in $ALLSIGLOG"
			notfoundsigs=`expr $notfoundsigs + 1`
			totalsigs=`expr $totalsigs + 1`
			nsig=`expr $nsig + 1`
			continue
			#exit 1;
		fi

		# 2nd: check freq
		if [ $trueavg -le $MINFREQ ]; then
			lowfreqsigs=`expr $lowfreqsigs + 1`
			totalsigs=`expr $totalsigs + 1`
			nsig=`expr $nsig + 1`
			continue
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

	# xgossip: subtract 2 dummies
	listsize=`expr $listsize - 2`
	if [ $listsize -eq 0 ]; then
		echo -n ",$lowfreqsigs,$totalsigs," >> $REPORT
	else
		meanabsrelerror=`echo "scale=$SCL; $absrelerror / $listsize * 100" | bc`
		echo -n "$meanabsrelerror,$lowfreqsigs,$totalsigs," >> $REPORT
	fi
	#echo -n "$meanabsrelerror, $notfoundsigs, $lowfreqsigs, $regularsigs, $multisets, $totalsigs, " >> $REPORT
	totalsigs=0
	notfoundsigs=0
	lowfreqsigs=0

	echo >> $REPORT

	echo $tid >> $ALLTEAMIDSLOG
	n=`expr $n + 1`
done

echo "teamIDs NOT found: $tidskipped/$NTEAMIDS"
