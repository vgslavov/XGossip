#!/bin/sh

COUNT=1
MOD=0
TOTALSLEEPTIME=0
AVGSLEEPTIME=0

ACTIVEHOSTS="hosts.active"
ALLHOSTS="hosts.all"
BOOTSTRAPHOST="hosts.bootstrap"
HOMEDIR="/home/vsfgd"
USER="umkc_rao1"
TMPFILE='.churn.tmp'

KICKSTARTPATH="/home/$USER/raopr/psix.new/scripts/kickstart"
LOGDIR="$HOMEDIR/log"

# ignore commented hosts
cat $ALLHOSTS | grep -v ^# > ${ALLHOSTS}.tmp
TOTALHOSTS=`wc -l < ${ALLHOSTS}.tmp`

TOTALACTIVEHOSTS=`wc -l < ${ACTIVEHOSTS}`

if [ $# -ne 1 ]; then
	echo -e 1>&2 "Usage: $0\t[churns per minute]"
	exit 127
fi

if [ ! -f $BOOTSTRAPHOST ]; then
	echo "no bootstrap host started"
	exit 127
fi

LOGFILE="$LOGDIR/churn.$1.log"

CHURN=$((60/$1))
# if multiple bootstrap hosts
BOOTSTRAP=`sed -n "1p" $BOOTSTRAPHOST`

# format
echo "current time,active hosts,churns/min,sleep time,total sleep time,avg sleep time" >> $LOGFILE

echo -n `date +%Y%m%d%H%M%S.%N` >> $LOGFILE
echo -n "," >> $LOGFILE
echo -n `wc -l < ${ACTIVEHOSTS}` >> $LOGFILE
echo -n "," >> $LOGFILE
echo $1 >> $LOGFILE

while [ 1 -eq 1 ]; do
	SLEEP=$(( $RANDOM % ($CHURN * 2)))
	echo "sleeping $SLEEP sec..."
	sleep $SLEEP
	TOTALSLEEPTIME=$(($TOTALSLEEPTIME + $SLEEP))
	AVGSLEEPTIME=`echo "scale=2;$TOTALSLEEPTIME/$COUNT" | bc`
	echo "Average Sleep Time Per Churn: $AVGSLEEPTIME"

	LINE_NUM=$(( $RANDOM % $TOTALHOSTS + 1 ))
	HOST=`sed -n "${LINE_NUM}p" ${ALLHOSTS}.tmp`
	# skip if it is bootstrap host
	if [ $HOST = $BOOTSTRAP ]; then
		continue
	fi
	echo "Selected peer: $HOST [$LINE_NUM]"
	ACTIVE=`grep ${HOST} ${ACTIVEHOSTS}`

	# is ACTIVE 0?
	if [ -z "$ACTIVE" ]; then
		echo "$HOST is not active and will be started"
		ssh $USER@$HOST $KICKSTARTPATH $BOOTSTRAP >& /dev/null &
		echo
		echo $HOST >> $ACTIVEHOSTS
	else
		echo "$ACTIVE is active and will be killed"
		ssh $USER@$HOST killall -s ABRT lsd adbd syncd >& /dev/null
		cat $ACTIVEHOSTS | grep -v $HOST > $TMPFILE
		mv $TMPFILE $ACTIVEHOSTS
		echo
	fi					

	echo -n `date +%Y%m%d%H%M%S.%N` >> $LOGFILE
	echo -n "," >> $LOGFILE
	echo -n `wc -l < ${ACTIVEHOSTS}` >> $LOGFILE
	echo -n "," >> $LOGFILE
	echo -n $1 >> $LOGFILE
	echo -n "," >> $LOGFILE
	echo -n $SLEEP >> $LOGFILE
	echo -n "," >> $LOGFILE
	echo -n $TOTALSLEEPTIME >> $LOGFILE
	echo -n "," >> $LOGFILE
	echo $AVGSLEEPTIME >> $LOGFILE

	COUNT=$(($COUNT + 1))
done
