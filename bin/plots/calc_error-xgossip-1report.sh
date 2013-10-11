#!/bin/bash

if [ $# -ne 6 ]; then
        echo "Usage: $0 [lists dir] [allsig log] [report] <round> <minfreq> [teamidnotfound log]"
        exit 1;
fi

DEBUG=0
USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
LISTDIR=${1}
# TODO: assumes listdir is named after host
PEER=`basename $LISTDIR`
ALLSIGLOG=${2}
REPORT=${3}
# process one specific round
ROUND=${4}
MINFREQ=${5}
TEAMIDNOTFOUNDLOG=${6}
# scale for bc
SCL=6

# TODO: use printlist value
TEAMSIZE=8
# the same dataset was used on each of the 4 nodes in the cluster
NDATASETS=20

if [ $DEBUG = 1 ]; then
	set -x
fi

# get a list of teamIDs
TEAMIDS=`ls $LISTDIR | awk -F . '{print $1}' | uniq`
# TODO: find a better way
NTEAMIDS=`ls $LISTDIR | awk -F . '{print $1}' | uniq | wc -l`
#echo "NTEAMIDS: " $NTEAMIDS

#nseq=-1
nseq=$ROUND
lowfreqsigs=0
notfoundsigs=0
totalsigs=0
tidskipped=0

#echo "peer,teamid,churn,sig,estimated count,true count,low freq,relative error" >> $REPORT
#echo "peer,teamid,churn,sig,estimated count,true count,relative error" >> $REPORT

for tid in $TEAMIDS ; do
	echo "teamID: " $tid

	ret=`grep -c $tid $TEAMIDNOTFOUNDLOG`
	if [ $ret -ne 0 ] ; then
		echo "skipping: teamID found in log"
		tidskipped=`expr $tidskipped + 1`
		churn="y"
	else
		churn="n"
	fi

	listhdr=`grep -m 1 "list T_0:" $LISTDIR/$tid.$nseq`
	regularsigs=0
	set -- `grep -m 1 "printlist: sumavg: " $LISTDIR/$tid.$nseq | awk '{print $7, $13}'`
	regularsigs=$1
	multisets=$2

	# TODO: needed?
	if [ -z "$regularsigs" ]; then
		continue
	fi

	listsize=`echo $listhdr | awk '{ print $8 }'`
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
			echo "$sig not found in $ALLSIGLOG"
			notfoundsigs=`expr $notfoundsigs + 1`
			totalsigs=`expr $totalsigs + 1`
			nsig=`expr $nsig + 1`
			#echo "sig not found in ALLSIGLOG," >> $REPORT
			continue
			#exit 1;
		fi
		
		# 2nd: check freq
		if [ $trueavg -le $MINFREQ ]; then
			#echo "skipping sig: freq=$trueavg"
			lowfreqsigs=`expr $lowfreqsigs + 1`
			#totalsigs=`expr $totalsigs + 1`
			#nsig=`expr $nsig + 1`
			#echo -n "y," >> $REPORT
			#continue
			#echo -n "n," >> $REPORT
		fi

		echo -n "$PEER," >> $REPORT
		echo -n "$tid," >> $REPORT
		echo -n "$churn," >> $REPORT
		echo -n "$sig," >> $REPORT
		echo -n "$avgp," >> $REPORT
		echo -n "$trueavg," >> $REPORT

		abserror=`echo "scale=$SCL; $avgp - $trueavg" | bc`
		# get absolute value
		abserror=`echo $abserror | awk ' { if($1>=0) { print $1 } else { print $1*-1 }}'`
		# convert awk's scientific notation to floating point
		abserror=`echo $abserror | awk -F"E" 'BEGIN{OFMT="%10.10f"} {print $1 * (10 ^ $2)}'`
		relerror=`echo "scale=$SCL; $abserror / $trueavg" | bc`
		# convert to %
		relerrorp=`echo "scale=$SCL; $relerror * 100" | bc`

		echo -n "$relerrorp," >> $REPORT
		echo >> $REPORT
		
		#absrelerror=`echo "scale=$SCL; $absrelerror + $relerror" | bc`
		
		totalsigs=`expr $totalsigs + 1`
		nsig=`expr $nsig + 1`
	done

	# xgossip: subtract 2 dummies
	#listsize=`expr $listsize - 2`
	#if [ $listsize -le 2 ] ; then
	#	meanabsrelerror=0
	#else
	#	meanabsrelerror=`echo "scale=$SCL; $absrelerror / $listsize * 100" | bc`
	#fi
	#echo -n "$meanabsrelerror, $lowfreqsigs, $totalsigs, " >> $REPORT
	#echo "meanabsrelerror (%): $meanabsrelerror"
	echo "notfoundsigs: $notfoundsigs"
	echo "totalsigs: $totalsigs"
	echo "lowfreqsigs: $lowfreqsigs"
	echo "teamIDs skipped: $tidskipped"
	totalsigs=0
	notfoundsigs=0
	lowfreqsigs=0
	tidskipped=0
done
