#!/bin/sh

#set -x

if [ $# -lt 11 ]; then
	echo "Usage: start_gpsi.sh <bcast|sigquery|xquery|vanilla|xinit|xexec> <total peers> <cluster nodes> <gossip interval> <listen time> <wait time> <querydir|sigdir|junk> <teamsize> <k> <l> <compress:0|1> [rounds]"
	exit 1;
fi

USER=`whoami`
HOME="/home/$USER"
TMP="$HOME/tmp"
ALLIDS="$TMP/allIDs.log"

PHASE=${1}
PEERS=${2}
CLUSTERNODES=${3}
GINTVAL=${4}
# 300, 1800
LISTENTIME=${5}
# 600, 1800
WAITTIME=${6}
DATADIR=${7}
TEAMSIZE=${8}
# k
BANDS=${9}
# l
ROWS=${10}
COMPRESS=${11}
ROUNDS=${12}

PRIME=1122941
BCAST=0
VANILLA=0
SIGQUERY=0
XQUERY=0
XINIT=0
XEXEC=0
INSERTTIME=2

# local
#GPSIPATH="$BINDIR/tools/gpsi"
# cluster/aws/plab
GPSIPATH="$HOME/raopr/psix.new/bin/gpsi"

# N peers on each CLUSTERNODE
# 0-based
#N=`expr $PEERS / $CLUSTERNODES - 1`
N=`expr $PEERS / $CLUSTERNODES`
echo $N

case "$PHASE" in
	(bcast) BCAST=1;;
	(sigquery) SIGQUERY=1;;
	(xquery) XQUERY=1;;
	(vanilla) VANILLA=1;;
	(xinit) XINIT=1;;
	(xexec) XEXEC=1;;
	(*) echo "not reached"; exit 127;;
esac

# which peer to use for querying
if [ $XQUERY = 1 ] ; then
	N=1
	$GPSIPATH -Q -S $TMP/$N/dhash-sock -x $QUERYDIR/ -L $TMP/log.query -d $PRIME -j $HOME/etc/irrpoly-deg9.dat -p -B $BANDS -R $ROWS -F $TMP/$N/hash.dat -Z $TEAMSIZE
	sleep 1
	exit;
fi

while [ $N -gt 0 ] ; do
	MYID=`grep myID $TMP/$N/log.lsd | awk '{print $4}'`
	#echo $MYID
	# broadcast
	if [ $BCAST = 1 ] ; then
		if [ $COMPRESS = 1 ] ; then
			$GPSIPATH -S $TMP/$N/dhash-sock -G $TMP/$N/g-sock -L $TMP/$N/log.gpsi -s $DATADIR/$N/ -g -t $GINTVAL -T $INSERTTIME -w $WAITTIME -W $LISTENTIME -p -q $PEERS -o $ROUNDS -N $CLUSTERNODES -U $MYID -i $ALLIDS -b -C &
		else
			$GPSIPATH -S $TMP/$N/dhash-sock -G $TMP/$N/g-sock -L $TMP/$N/log.gpsi -s $DATADIR/$N/ -g -t $GINTVAL -T $INSERTTIME -w $WAITTIME -W $LISTENTIME -p -q $PEERS -o $ROUNDS -N $CLUSTERNODES -U $MYID -i $ALLIDS -b &
		fi
	# vanillaxgossip
	elif [ $VANILLA = 1 ] ; then
		if [ $COMPRESS = 1 ] ; then
			$GPSIPATH -S $TMP/$N/dhash-sock -G $TMP/$N/g-sock -L $TMP/$N/log.gpsi -s $DATADIR/$N/ -g -t $GINTVAL -T $INSERTTIME -w $WAITTIME -W $LISTENTIME -p -q $PEERS -o $ROUNDS -N $CLUSTERNODES -C &
		else
			$GPSIPATH -S $TMP/$N/dhash-sock -G $TMP/$N/g-sock -L $TMP/$N/log.gpsi -s $DATADIR/$N/ -g -t $GINTVAL -T $INSERTTIME -w $WAITTIME -W $LISTENTIME -p -q $PEERS -o $ROUNDS -N $CLUSTERNODES &
		fi
	# xgossip init
	elif [ $XINIT = 1 ] ; then
		$GPSIPATH -S $TMP/$N/dhash-sock -G $TMP/$N/g-sock -L $TMP/$N/log.gpsi -s $DATADIR/$N/ -g -t $GINTVAL -T $INSERTTIME -w $WAITTIME -W $LISTENTIME -H -d $PRIME -j $HOME/etc/irrpoly-deg9.dat -I -p -P $TMP/$N/log.init -B $BANDS -R $ROWS -F $TMP/$N/hash.dat -Z $TEAMSIZE -q $PEERS -N $CLUSTERNODES &
	# xgossip exec
	elif [ $XEXEC = 1 ] ; then
		if [ $COMPRESS = 1 ] ; then
			$GPSIPATH -S $TMP/$N/dhash-sock -G $TMP/$N/g-sock -L $TMP/$N/log.gpsi -g -t $GINTVAL -T $INSERTTIME -w $WAITTIME -W $LISTENTIME -H -d $PRIME -j $HOME/etc/irrpoly-deg9.dat -E -p -P $TMP/$N/log.init -B $BANDS -R $ROWS -F $TMP/$N/hash.dat -Z $TEAMSIZE -q $PEERS -o $ROUNDS -N $CLUSTERNODES -C &
		else
			$GPSIPATH -S $TMP/$N/dhash-sock -G $TMP/$N/g-sock -L $TMP/$N/log.gpsi -g -t $GINTVAL -T $INSERTTIME -w $WAITTIME -W $LISTENTIME -H -d $PRIME -j $HOME/etc/irrpoly-deg9.dat -E -p -P $TMP/$N/log.init -B $BANDS -R $ROWS -F $TMP/$N/hash.dat -Z $TEAMSIZE -q $PEERS -o $ROUNDS -N $CLUSTERNODES &
		fi
	# query
	elif [ $SIGQUERY = 1 ] ; then
		$GPSIPATH -Q -S $TMP/$N/dhash-sock -s $DATADIR/$N/ -L $TMP/$N/log.query -d $PRIME -j $HOME/etc/irrpoly-deg9.dat -p -B $BANDS -R $ROWS -F $TMP/$N/hash.dat -Z $TEAMSIZE
		sleep 1
	fi

	#sleep 5
	N=`expr $N - 1`
done
