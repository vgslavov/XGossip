#!/bin/bash

if [ $# -ne 9 ]; then
	echo "Usage: $0 [lists dir] [all sigs log] [reports dir] [teamID-NOT-found log] [all teamIDs] <rounds> <min freq> <team size> <# of instances>"
	exit 1;
fi

USER=`whoami`
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/`basename $0`.tmp"
LISTDIR=${1}
ALLSIGLOG=${2}
REPORTDIR=${3}
TEAMIDNOTFOUNDLOG=${4}
ALLTEAMIDSLOG=${5}
ROUNDS=${6}
MINFREQ=${7}
# TODO: use printlist value
TEAMSIZE=${8}
# the same dataset was used on each of the N nodes in the cluster
NDATASETS=${9}
# all $ROUNDS
#CALCERROR="$HOMEDIR/bin/plots/calc_error-xgossip.sh"
# last round only
CALCERROR="$HOMEDIR/bin/plots/calc_error-xgossip-last.sh"

#set -x

for i in `ls -d $LISTDIR/*` ; do
	HOST=`basename $i`
	echo $HOST
	$CALCERROR $i $ALLSIGLOG $REPORTDIR/$HOST-error-min${MINFREQ}r${ROUNDS}.csv $TEAMIDNOTFOUNDLOG $ALLTEAMIDSLOG $ROUNDS $MINFREQ $TEAMSIZE $NDATASETS
done
