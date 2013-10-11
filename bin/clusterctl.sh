#!/bin/sh

# $Id: plabctl.sh,v 1.1 2010/02/19 02:43:30 vsfgd Exp vsfgd $

# hosts
ACTIVEHOSTS="hosts.active"
ALLHOSTS="hosts.all"
BOOTSTRAPHOST="hosts.bootstrap"
CLUSTER="cluster"
CURRTIME=`date +%Y%m%d%H%M%S`
DOWNHOSTS="hosts.down"
NOFILESHOSTS="hosts.nofiles"
ERRORHOSTS="hosts.errors"
GOODHOSTS="hosts.good"
WHATHOSTS="hosts.what"

# paths
LOCALUSER="vsfgd"
LOCALHOME="/home/$LOCALUSER"
BUILDHOME="$LOCALHOME/build/xgossip/psix.new.tar.gz"
DATAFILE="many2.dtd-13highsim-hosts100.tgz"
#DATAFILE="many.dtd-hosts100-aws2.tgz"
#DATAFILE="many.dtd-hosts200.tgz"
DATAHOME="$LOCALHOME/sig/$DATAFILE"
GPSIBIN="$LOCALHOME/build/xgossip/psix.new/bin/gpsi"
USER="vsfgd"
#USER="ec2-user"
SCRIPTSHOME="$LOCALHOME/bin/awsbin"
CONFIGHOME="/home/umkc_rao2/raopr/psix.new/"
POLYPATH="$LOCALHOME/etc/irrpoly-deg9.dat"
#REMOTEHOME="/$USER"
REMOTEHOME="/home/$USER"
PSIXHOME="$REMOTEHOME/raopr/psix.new"
CONFIGPATH="$PSIXHOME/psix.config"
KICKSTARTPATH="$PSIXHOME/scripts/kickstart"
GPSIPATH="$PSIXHOME/bin/gpsi"
SOCKPATH="$PSIXHOME/tmp/lsd-1"

# ssh
KEY="$LOCALHOME/.ssh/umkc-cluster"
SCP="scp -i $KEY -P"
SSH="ssh -i $KEY -p"

# XGossip
GPSICMD="$GPSIPATH -S $SOCKPATH/dhash-sock -G $SOCKPATH/g-sock -L $SOCKPATH/log.gpsi -s $REMOTEHOME/sig-netdb-100/ -g -t 120 -p -q 100 -X 3"

# XGossip+
# init phase
#GPSICMD="$GPSIPATH -S $SOCKPATH/dhash-sock -G $SOCKPATH/g-sock -L $SOCKPATH/log.gpsi -s $REMOTEHOME/sig-gauss-149/ -g -t 240 -T 2 -w 1800 -H -d 1122941 -j $REMOTEHOME/irrpoly-deg9.dat -u -B 10 -R 16 -I -p -P $SOCKPATH/log.init -F $REMOTEHOME/hash.dat -q 92"
# exec phase
#GPSICMD="$GPSIPATH -S $SOCKPATH/dhash-sock -G $SOCKPATH/g-sock -L $SOCKPATH/log.gpsi -g -t 240 -T 2 -w 120 -H -d 1122941 -j $REMOTEHOME/irrpoly-deg9.dat -u -B 10 -R 16 -E -p -P $REMOTEHOME/log/20101127i/log.init -F $REMOTEHOME/hash.dat -q 91"

# start chord manually
#ssh umkc_rao1@planetlab-2.cmcl.cs.cmu.edu "/home/umkc_rao1/raopr/psix.new/scripts/kickstart planet6.berkeley.intel-research.net" &
# start gpsi manually
#/home/umkc_rao1/raopr/psix.new/bin/gpsi -S /home/umkc_rao1/raopr/psix.new/tmp/lsd-1/dhash-sock -G /home/umkc_rao1/raopr/psix.new/tmp/lsd-1/g-sock -L /home/umkc_rao1/raopr/psix.new/tmp/lsd-1/log.gpsi -s /home/umkc_rao1/sig-gauss-188/ -g -i 120 -p -q 188 &

ACTIVE=0
ALL=0
DO=0
CONFIG=0
DEBUG=0
GET=0
HOST=0
INSTALL=0
UPLOAD=0
KILL=0
LINE_NUM=0
NUM=0
OTHER=0
PS=0
REMOTEALL=0
REMOTEPROC=0
START=0
TEST=0
n=0

cat $ALLHOSTS | grep -v ^# > ${ALLHOSTS}.tmp
TOTALHOSTS=`wc -l < ${ALLHOSTS}.tmp`

if [ $# -lt 2 -o $# -gt 3 ]; then
	echo -e 1>&2 "Usage: $0\t[config]\t[all | active | host]"
	echo -e 1>&2 "\t\t\t[do]\t\t[all | active | host] [cmd]"
	echo -e 1>&2 "\t\t\t[get]\t\t[all | active | host] [gpsi | lsd | init]"
	echo -e 1>&2 "\t\t\t[install]\t[all | active | host]"
	echo -e 1>&2 "\t\t\t[upload]\t[all | active | host] [all | gpsi | files ]"
	echo -e 1>&2 "\t\t\t[kill]\t\t[all | active | host] [all | gpsi | all]"
	echo -e 1>&2 "\t\t\t[ps]\t\t[all | active | host] [all | gpsi | lsd]"
	echo -e 1>&2 "\t\t\t[start]\t\t[all | active | host] [gpsi | lsd]"
	echo -e 1>&2 "\t\t\t[test]\t\t[all | active | host]"
	exit 127
fi

case "$1" in
	(kill) KILL=1;;
	(start) START=1;;
	(ps) PS=1;;
	(config) CONFIG=1;;
	(install) INSTALL=1;;
	(upload) UPLOAD=1;;
	(test) TEST=1;;
	(do) DO=1;;
	(get) GET=1;;
	(*) echo "not reached"; exit 127;;
esac

case "$2" in
	(all) ALL=1; NUM=$TOTALHOSTS;;
	(active) ACTIVE=1;;
	(*) OTHER=1;;
esac

if [ $# -ne 3 -a $GET = 1 ]; then
	echo "missing parameter"
	exit 127
elif [ $# -ne 3 -a $PS = 1 ]; then
	echo "missing parameter"
	exit 127
elif [ $# -ne 3 -a $KILL = 1 ]; then
	echo "missing parameter"
	exit 127
elif [ $# -ne 3 -a $UPLOAD = 1 ]; then
	echo "missing parameter"
	exit 127
elif [ $# -ne 3 -a $DO = 1 ]; then
	echo "missing parameter"
	exit 127
elif [ $# -ne 3 -a $START = 1 ]; then
	echo "missing parameter"
	exit 127
fi

if [  $# -eq 3 ]; then
	case "$3" in
		(gpsi) REMOTEPROC="gpsi";;
		(lsd) REMOTEPROC="lsd";;
		(init) REMOTEPROC="init";;
		(all) REMOTEALL=1;;
		#(*) echo "not reached"; exit 127;;
		(*) CMD=$3;;
	esac
fi

if [ $DEBUG = 1 ]; then
	set -x
fi

if [ $START = 1 -a $REMOTEPROC = "lsd" ] ; then
	# start on one specific host
	if [ $OTHER = 1 ] ; then
		# random
		#if [ $2 -eq $2 2> /dev/null ] ; then
			# choose random bootstrap LINE_NUM=$(( $RANDOM % $TOTALHOSTS + 1 ))
			#BOOTSTRAP=`sed -n "${LINE_NUM}p" ${ALLHOSTS}.tmp`
			#echo -n "[$LINE_NUM] "
			# one less because of bootstrap
			#NUM=`expr $2 - 1`;
		# one host
		#else
		HOST=$2
		if [ -f $BOOTSTRAPHOST ] ; then
			BOOTSTRAP=`cat $BOOTSTRAPHOST`
			echo "BOOTSTRAP=$BOOTSTRAP"
			echo
			echo "starting Chord on $HOST..."
			ssh $USER@$HOST $KICKSTARTPATH $BOOTSTRAP &
			#echo $HOST >> $ACTIVEHOSTS
			exit
		else
			BOOTSTRAP=$2
		fi
		# while loop will never execute because NUM is init to 0
		#fi
	# start on all
	elif [ $ALL = 1 ] ; then
		# choose host on last line of $ALLHOSTS
		BOOTSTRAP=`sed -n "${NUM}p" ${ALLHOSTS}.tmp`
		NUM=`expr $NUM - 1`;
	fi

	echo "BOOTSTRAP=$BOOTSTRAP"
	echo
	#echo "($LINE_NUM) killing psiX on $BOOTSTRAP..."
	#echo
	#ssh $USER@$BOOTSTRAP killall -s ABRT lsd adbd syncd
	echo "starting Chord on BOOTSTRAP..."
	ssh $USER@$BOOTSTRAP $KICKSTARTPATH $BOOTSTRAP &
	#echo $BOOTSTRAP >> $ACTIVEHOSTS
	echo $BOOTSTRAP > $BOOTSTRAPHOST
	echo "sleeping for 30 seconds..."
	sleep 30
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

		##echo "killing psiX on $HOST..."
		##echo
		##ssh $USER@$HOST killall -s ABRT lsd adbd syncd
		echo "$HOST (startlsd):"
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
		# don't skip bootstrap when using active list only
		# uncomment if hosts.active is not a copy of hosts.lsd!
		#if [ $START = 1 -a $REMOTEPROC = "gpsi" ] ; then
			#echo " (start $REMOTEPROC on BOOTSTRAP): "
			#ssh $USER@$BOOTSTRAPHOST $GPSICMD &
			#n=`expr $n + 1`;
		#fi
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
			if [ $REMOTEALL = 1 ] ; then
				$SSH $i $USER@$CLUSTER killall -s ABRT lsd adbd syncd gpsi
			elif [ $REMOTEPROC = "gpsi" ] ; then
				$SSH $i $USER@$CLUSTER killall -s ABRT gpsi
			elif [ $REMOTEPROC = "lsd" ] ; then
				$SSH $i $USER@$CLUSTER killall -s ABRT lsd adbd syncd
			else
				echo "something is wrong"
			fi
			n=`expr $n + 1`;
		elif [ $START = 1 -a $REMOTEPROC = "gpsi" ] ; then
			echo " (start $REMOTEPROC): "
			$SSH $i $USER@$CLUSTER $GPSICMD &
			n=`expr $n + 1`;
			#echo "sleeping for 5 seconds..."
			#sleep 5
		elif [ $PS = 1 ] ; then
			echo -n " (ps): "
			if [ $REMOTEALL = 1 ] ; then
				$SSH $i $USER@$CLUSTER ps xw
				ret="-10"
				n=`expr $n + 1`;
			elif [ $REMOTEPROC = "gpsi" ] ; then
				# put 1st char in [] to exclude grep
				#ret=`ssh ${USER}@${i} ps xw|grep [g]psi >/dev/null; echo $?`
				ret=`$SSH $i $USER@$CLUSTER ps xw|grep [g]psi|wc -l`
				echo $ret
				n=`expr $n + $ret`;
			elif [ $REMOTEPROC = "lsd" ] ; then
				#ret=`ssh ${USER}@${i} ps xw|grep [l]sd >/dev/null; echo $?`
				ret=`$SSH $i $USER@$CLUSTER ps xw|grep [s]yncd|wc -l`
				echo $ret
				n=`expr $n + $ret`;
			else
				echo "something is wrong"
			fi
			#PSFILE="hosts.ps-$CURRTIME.$REMOTEPROC"
			#PSFILEDOWN="hosts.ps-$CURRTIME.$REMOTEPROC.down"
			# if host is responding
			#if [ $ret -eq 255 ]; then
				#echo "host is down"
			#elif [ $ret -eq 0 ]; then
				#echo "$REMOTEPROC is running"
				#echo $i >> $PSFILE
				#n=`expr $n + 1`;
			#elif [ $ret -eq 1 ]; then
				#echo "$REMOTEPROC is NOT running"
				#echo $i >> $PSFILEDOWN
			# if REMOTEALL
			#elif [ $ret -eq -10 ]; then
				#echo -n ""
			#else
				#echo "something is wrong"
			#fi
		elif [ $UPLOAD = 1 ] ; then
			echo ' (upload):'
			if [ $REMOTEALL = 1 ] ; then
				$SCP $i $BUILDHOME $USER@$CLUSTER:~/
			elif [ $REMOTEPROC = "gpsi" ] ; then
				$SCP $i $GPSIBIN $USER@$CLUSTER:$PSIXHOME/bin/
			else
				#$SSH $i $USER@$CLUSTER "mkdir ~/bin; mkdir ~/tmp; mkdir ~/etc; mkdir ~/sig; mkdir ~/log"
				$SCP $i $SCRIPTSHOME/* $USER@$CLUSTER:~/bin
				#$SCP $i $SCRIPTSHOME/.bashrc $USER@$CLUSTER:~/
				#$SCP $i $POLYPATH $USER@$CLUSTER:~/etc/
				#$SCP $i $DATAHOME $USER@$CLUSTER:~/sig/
			fi
			n=`expr $n + 1`;
		elif [ $INSTALL = 1 ] ; then
			echo ' (install):'
			$SSH $i $USER@$CLUSTER "mkdir -p raopr; mv psix.new.tar.gz raopr/; cd raopr; tar xvfz psix.new.tar.gz; mv psix.new.tar.gz ../; cd ..;"
			#$SSH $i $USER@$CLUSTER "cd ~/sig; tar zxf ~/sig/$DATAFILE"
			n=`expr $n + 1`;
		elif [ $TEST = 1 ] ; then
			echo -n " (test): "
			$SSH $i $USER@$CLUSTER $GPSIPATH -v

			# generic test
			#ret=`ssh ${USER}@${i} uname >/dev/null; echo $?`
			# test for psiX binaries
			#ret=`ssh ${USER}@${i} ls -l $KICKSTARTPATH >/dev/null; echo $?`
			# test for psix.config
			#ret=`ssh ${USER}@${i} ls -l $CONFIGPATH >/dev/null; echo $?`
			# test for sig
			#ret=`ssh ${USER}@${i} ls -l sig >/dev/null; echo $?`
			#echo " (test): ${ret}"
			#if [ $ret -ne 255 ]; then
			#if [ $ret -eq 255 ]; then
				#echo "host is down"
				#echo $i >> $DOWNHOSTS-$CURRTIME
			#elif [ $ret -eq 2 ]; then
				#echo "host is missing psiX"
				#echo $i >> $NOFILESHOSTS-$CURRTIME
			#elif [ $ret -eq 1 ]; then
				#echo "vserver error"
				#echo $i >> $ERRORHOSTS-$CURRTIME
			#elif [ $ret -eq 0 ]; then
				#echo "host is up and running"
				#echo $i >> $GOODHOSTS-$CURRTIME
				#n=`expr $n + 1`;
			#else
				#echo "what is up with this host?"
				#echo -n $i >> $WHATHOSTS-$CURRTIME
				#echo " " $ret >> $WHATHOSTS-$CURRTIME
			#fi
		elif [ $DO = 1 ] ; then
			echo ' (do): '
			#ssh $USER@$i uptime
			#ssh $USER@$i ls sig
			#ssh $USER@$i rm $PSIXHOME/tmp/lsd-1/log.gpsi &
			$SSH $i $USER@$CLUSTER $CMD
			#$SSH $i $USER@$CLUSTER $GPSIPATH -v
			n=`expr $n + 1`;
		elif [ $GET = 1 ] ; then
			echo " (get $REMOTEPROC):"
			$SCP $i $USER@$CLUSTER:~/log/20111003-exec-k5l10s8.tgz $LOCALHOME/log/504cluster/$i-20111003-exec-k5l10s8.tgz
			#$SCP $i $USER@$CLUSTER:$PSIXHOME/tmp/lsd-1/log.$REMOTEPROC $LOCALHOME/log/log.$i.$REMOTEPROC &
			n=`expr $n + 1`;
		elif [ $CONFIG = 1 ] ; then
			echo ' (config):'
			ssh $USER@$i cat $CONFIGPATH
			ssh $USER@$i sed -i -e "s/MAXRETRIEVERETRIES\ 2/MAXRETRIEVERETRIES\ 2/g" $CONFIGPATH
			ssh $USER@$i sed -i -e "s/dhash.replica\ 12/dhash.replica\ 6/g" $CONFIGPATH
			ssh $USER@$i cat $CONFIGPATH
			n=`expr $n + 1`;
		fi
		#echo -ne "\n"
	done

	# kill only all or active
	#if [ $KILL = 1 -a $OTHER = 0 ] ; then
		#BOOTSTRAP=`cat $BOOTSTRAPHOST`
		#echo "killing process on bootstrap ($BOOTSTRAP)..."
		#if [ $REMOTEALL = 1 ] ; then
			#ssh $USER@$BOOTSTRAP killall -s ABRT lsd adbd syncd gpsi
			#mv $ACTIVEHOSTS $ACTIVEHOSTS-$CURRTIME
			#mv $BOOTSTRAPHOST $BOOTSTRAPHOST-$CURRTIME
		#elif [ $REMOTEPROC = "gpsi" ] ; then
			#ssh $USER@$BOOTSTRAP killall -s ABRT gpsi
		#elif [ $REMOTEPROC = "lsd" ] ; then
			#ssh $USER@$BOOTSTRAP killall -s ABRT lsd adbd syncd
			#mv $ACTIVEHOSTS $ACTIVEHOSTS-$CURRTIME
			#mv $BOOTSTRAPHOST $BOOTSTRAPHOST-$CURRTIME
		#else
			#echo "something is wrong"
		#fi
	#fi

	echo "$n hosts successful"
fi
#set +x

#rm ${ALLHOSTS}.tmp
