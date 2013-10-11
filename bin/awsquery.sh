#!/bin/sh

if [ $# -ne 10 ]; then
	echo "Usage: $0 <query dir> <proxy dir> <dataset dir> <results dir> <k> <l> <team size> <peers> <instances> <output log>"
	exit 1;
fi

QUERYSET="$1"
PROXYSET="$2"
DATASET="$3"
RESULTS="$4"
K="$5"
L="$6"
TEAMSIZE="$7"
PEERS="$8"
INSTANCES="$9"
OUTLOG="$10"

LOCALUSER="ec2-user"
LOCALHOME="/home/$LOCALUSER"
REMOTEUSER="ec2-user"
REMOTEHOME="/home/$REMOTEUSER"
GPSIBIN="$REMOTEUSER/raopr/psix.new/bin/gpsi"
AWSCTL="$LOCALHOME/bin/awsctl.sh"
TMP="$REMOTEHOME/tmp"
PRIME=1122941

# send query to an instance running XGossip
$GPSIBIN -Q -S $TMP/1/dhash-sock -O $PROXYSET -x $QUERYSET -F $TMP/1/hash.dat -d $PRIME -j $REMOTEHOME/etc/irrpoly-deg9.dat -B $K -R $L -Z $TEAMSIZE -p -T 1 -L $TMP/log.queryxp

# extract query results from all instances running XGossip
# TODO: extract only from the one which had results (use LSH)
$AWSCTL all do "mkdir -p ~/log/results"
$AWSCTL all do "$REMOTEHOME/bin/extract_results-mod.sh gpsi ~/log/results/ junk"

# get query results from all instances running XGossip to the processing one
$AWSCTL all get results

# combine results, compare with true count, and calculate relative error
$GPSIBIN -M -D $RESULTS -s $DATASET -O $PROXYSET -y results -Z $TEAMSIZE -L $OUTLOG -j $LOCALHOME/etc/irrpoly-deg9.dat -F $LOCALHOME/etc/hash.dat -d $PRIME -B $K -R $L -N $INSTANCES -q $PEERS
