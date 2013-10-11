#!/bin/sh

ACTIVEHOSTS="hosts.active"
ALLHOSTS="hosts.all"
BOOTSTRAPHOST="hosts.bootstrap"
CURRTIME=`date +%Y%m%d%H%M%S`
DOWNHOSTS="hosts.down"
NOFILESHOSTS="hosts.nofiles"
USER="umkc_rao1"

CONFIGPATH="/home/$USER/raopr/psix.new/psix.config"
KICKSTARTPATH="/home/$USER/raopr/psix.new/scripts/kickstart"

ACTIVE=0
ALL=0
CMD=0
CONFIG=0
DEBUG=0
HOST=0
INSTALL=0
KILL=0
LINE_NUM=0
NUM=0
OTHER=0
PS=0
START=0
TEST=0
n=0

cat $ALLHOSTS | grep -v ^# > ${ALLHOSTS}.tmp
TOTALHOSTS=`wc -l < ${ALLHOSTS}.tmp`

if [ $# -ne 2 ]; then
	echo -e 1>&2 "Usage: $0\t[kill]\t\t[all | active | host]"
	echo -e 1>&2 "\t\t\t[start]\t\t[all | <num> | host]"
	echo -e 1>&2 "\t\t\t[ps]\t\t[all | active | host]"
	echo -e 1>&2 "\t\t\t[config]\t[all | active | host]"
	echo -e 1>&2 "\t\t\t[install]\t[all | active | host]"
	echo -e 1>&2 "\t\t\t[test]\t\t[all | active | host]"
	echo -e 1>&2 "\t\t\t[cmd]\t\t[all | active | host]"
	exit 127
fi

case "$1" in
	(kill) KILL=1;;
	(ps) PS=1;;
	(config) CONFIG=1;;
	(install) INSTALL=1;;
	(start) START=1;;
	(test) TEST=1;;
	(cmd) CMD=1;;
	(*) echo "not reached"; exit 127;;
esac

case "$2" in
	(all) ALL=1; NUM=$TOTALHOSTS;;
	(active) ACTIVE=1;;
	(*) OTHER=1;;
esac

if [ DEBUG = 1 ]; then
	set -x
fi

if [ $START = 1 ] ; then
	# start on a random number of hosts or on one specific host
	if [ $OTHER = 1 ] ; then
		# random
		if [ $2 -eq $2 2> /dev/null ] ; then
			# choose random bootstrap
			LINE_NUM=$(( $RANDOM % $TOTALHOSTS + 1 ))
			BOOTSTRAP=`sed -n "${LINE_NUM}p" ${ALLHOSTS}.tmp`
			echo -n "[$LINE_NUM] "
			# one less because of bootstrap
			NUM=`expr $2 - 1`;
		# one host
		else
			BOOTSTRAP=$2
			# while loop will never execute because NUM is init to 0
		fi
	# start on all
	elif [ $ALL = 1 ] ; then
		# choose host on last line of $ALLHOSTS
		BOOTSTRAP=`sed -n "${NUM}p" ${ALLHOSTS}.tmp`
		NUM=`expr $NUM - 1`;
	fi

	echo "BOOTSTRAP=$BOOTSTRAP"
	echo
	echo "starting psiX on BOOTSTRAP..."
	ssh $USER@$BOOTSTRAP $KICKSTARTPATH $BOOTSTRAP &
	echo $BOOTSTRAP > $BOOTSTRAPHOST
	echo

	while [ $NUM -gt 0 ] ; do
		# random
		if [ $ALL = 0 ] ; then
			LINE_NUM=$(( $RANDOM % $TOTALHOSTS + 1 ))
			HOST=`sed -n "${LINE_NUM}p" ${ALLHOSTS}.tmp`
			ret=`grep $HOST $ACTIVEHOSTS > /dev/null; echo $?`
			if [ $HOST = $BOOTSTRAP ]; then
		                ret=0
        		fi
			# FIXME: does HOST in while condition change?
			# grep: if status is 0, host is found
			while [ $ret -eq 0 ] ; do
				LINE_NUM=$(( $RANDOM % $TOTALHOSTS + 1 ))
				HOST=`sed -n "${LINE_NUM}p" ${ALLHOSTS}.tmp`
				ret=`grep $HOST $ACTIVEHOSTS > /dev/null; echo $?`
			done
			echo -n "[$LINE_NUM] "
		elif [ $ALL = 1 ] ; then
			HOST=`sed -n "${NUM}p" ${ALLHOSTS}.tmp`
			echo -n "[$NUM] "
		fi

		echo "$HOST (start):"
		ssh $USER@$HOST $KICKSTARTPATH $BOOTSTRAP &
		echo
		echo $HOST >> $ACTIVEHOSTS
		NUM=`expr $NUM - 1`
	done
else
	if [ $ALL = 1 ] ; then
		LIST=`cat ${ALLHOSTS}.tmp`
	elif [ $OTHER = 1 ] ; then
		LIST=$2
	elif [ -f $ACTIVEHOSTS ] ; then
		LIST=`cat ${ACTIVEHOSTS}`
	else
		echo "no active hosts"
		exit 127
	fi
	
	for i in $LIST; do
		# skip commented hosts
		if [ $(echo ${i} | grep ^# > /dev/null; echo $?) -eq 0 ]; then
			continue
		fi

		echo -n $i

		if [ $KILL = 1 ] ; then
			echo ' (kill):'
			ssh $USER@$i killall -s ABRT lsd adbd syncd
		elif [ $PS = 1 ] ; then
			echo " (ps): "
			ssh $USER@$i ps xw
		elif [ $INSTALL = 1 ] ; then
			echo ' (install):'
			# install new binary for config file fix (2nd)
			scp ~/var/bin.tar.gz $USER@$i:~/raopr/psix.new
			ssh $USER@$i "cd raopr/psix.new; tar xvfz bin.tar.gz;"
			# install psix (1st)
			#scp ~/var/psix.new.tar.gz $USER@$i:~/
			#ssh $USER@$i "mkdir -p raopr; mv psix.new.tar.gz raopr/; cd raopr; tar xvfz psix.new.tar.gz; mv psix.new.tar.gz ../; cd ..;"
		elif [ $TEST = 1 ] ; then
			# generic test
			ret=`ssh ${USER}@${i} uname >/dev/null; echo $?`
			# test for psiX binaries
			#ret=`ssh ${USER}@${i} ls -l $KICKSTARTPATH >/dev/null; echo $?`
			# test for psix.config
			#ret=`ssh ${USER}@${i} ls -l $CONFIGPATH >/dev/null; echo $?`
			echo " (config): ${ret}"
			if [ $ret -eq 255 ]; then
				echo "host is down"
				echo $i >> $DOWNHOSTS-$CURRTIME
			elif [ $ret -eq 2 ]; then
				echo "host is missing psiX"
				echo $i >> $NOFILESHOSTS-$CURRTIME
			else
				echo "host is up and no files are missing"
			fi
		elif [ $CMD = 1 ] ; then
			echo ' (cmd):'
			ssh $USER@$i uptime
		elif [ $CONFIG = 1 ] ; then
			echo ' (config):'
			ssh $USER@$i cat $CONFIGPATH
			ssh $USER@$i sed -i -e "s/MAXRETRIEVERETRIES\ 2/MAXRETRIEVERETRIES\ 2/g" $CONFIGPATH
			ssh $USER@$i sed -i -e "s/dhash.replica\ 12/dhash.replica\ 6/g" $CONFIGPATH
			ssh $USER@$i cat $CONFIGPATH
		fi
		#echo -ne "\n"
	done

	# kill only all or active
	if [ $KILL = 1 -a $OTHER = 0 ] ; then
		BOOTSTRAP=`cat $BOOTSTRAPHOST`
		echo "killing psiX on bootstrap ($BOOTSTRAP)..."
		ssh $USER@$BOOTSTRAP killall -s ABRT lsd adbd syncd
		mv $ACTIVEHOSTS $ACTIVEHOSTS-$CURRTIME
		mv $BOOTSTRAPHOST $BOOTSTRAPHOST-$CURRTIME
	fi

	if [ $TEST = 1 ] ; then
		echo "$n hosts responded"
	fi
fi
