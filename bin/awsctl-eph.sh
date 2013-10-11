#!/bin/sh

# $Id: plabctl.sh,v 1.1 2010/02/19 02:43:30 vsfgd Exp vsfgd $
#SCRIPT_LOCATION=`dirname $0`;
#cd $SCRIPT_LOCATION;

# files
ACTIVEHOSTS="hosts.active"
ALLHOSTS="hosts.8000"
BOOTSTRAPHOST="hosts.bootstrap"
CURRTIME=`date +%Y%m%d%H%M%S`
DOWNHOSTS="hosts.down"
NOFILESHOSTS="hosts.nofiles"
ERRORHOSTS="hosts.errors"
GOODHOSTS="hosts.good"
WHATHOSTS="hosts.what"

# paths
#LOCALUSER=`whoami`
LOCALUSER=vsfgd
USER="ec2-user"
LOCALHOME="/home/$LOCALUSER"
BUILDHOME="$LOCALHOME/build/xgossip/psix.new.tar.gz"
#DATADIR="many2.dtd-11highsim-hosts50-hostsperdtd25"
#DATADIR="many2.dtd-11highsim-hosts100-hostsperdtd50"
#DATADIR="many2.dtd-13highsim-hosts25-hostsperdtd13"
#DATADIR="many2.dtd-13highsim-hosts50-hostsperdtd25"
#DATADIR="many2.dtd-13highsim-hosts100-hostsperdtd50"
#DATADIR="many2.dtd-13highsim-hosts200-hostsperdtd100"
DATADIR="many2.dtd-13highsim-hosts400-hostsperdtd200"
DATAFILE="$DATADIR.tgz"
DATAHOME="$LOCALHOME/sig/$DATAFILE"
GPSIBIN="$LOCALHOME/build/xgossip/psix.new/bin/gpsi"
SCRIPTSHOME="$LOCALHOME/bin/awsbin.eph"
CONFIGHOME="/home/umkc_rao2/raopr/psix.new/"
POLYPATH="$LOCALHOME/etc/irrpoly-deg9.dat"
#REMOTEHOME="/$USER"
REMOTEHOME="/home/$USER"
REMOTETMP="/media/ephemeral0/tmp"
REMOTETMPL="/media/ephemeral0/tmp.churn.lsd"
REMOTETMPG="/media/ephemeral0/tmp.churn.gpsi"
PSIXHOME="$REMOTEHOME/raopr/psix.new"
CONFIGPATH="$PSIXHOME/psix.config"
KICKSTARTPATH="$PSIXHOME/scripts/kickstart"
GPSIPATH="$PSIXHOME/bin/gpsi"
SOCKPATH="$PSIXHOME/tmp/lsd-1"
SESSIONSDIR="$LOCALHOME/etc/sessions"
KILLDIR="$LOCALHOME/etc/killed"

# values
TOTALPEERS=8000
TOTALCHURNPEERS=1200
SESSIONSFILE="$REMOTEHOME/etc/sessions-$TOTALCHURNPEERS.log"
KILLFILE="$REMOTEHOME/etc/killed-$TOTALCHURNPEERS.log"
KILLRANGE=20
INSTANCES=20
ROUNDS=20
COMPRESSION=1
BANDS=8
ROWS=10
TEAMSIZE=8
# bcast
#GOSSIPINTERVAL=10
# vanilla/xgossip
#GOSSIPINTERVAL=120
#GOSSIPINTERVAL=240
GOSSIPINTERVAL=300
PEERS=`expr $TOTALPEERS \/ $INSTANCES`
CHURNPEERS=`expr $TOTALCHURNPEERS \/ $INSTANCES`
BOOTPORT=9555

# ec2
LISTENTIME=300
# cluster
#LISTENTIME=1800

# 1000 peers
# init
WAITTIME=1800
# exec?
#WAITTIME=2400
# 500 peers
# init
#WAITTIME=5000

# commands
# start chord
LSDCMD="cd $REMOTETMP; ~/bin/run_peers.sh $PEERS $BOOTPORT $BOOTPORT 0"

# start VanillaXGossip
# (values of the following don't matter: teamsize, bands, rows)
#GPSICMD="~/bin/start_gpsi.sh vanilla $TOTALPEERS $INSTANCES $GOSSIPINTERVAL $LISTENTIME $WAITTIME ~/sig/$DATADIR/ $TEAMSIZE $BANDS $ROWS $COMPRESSION $ROUNDS"

# start XGossip init
GPSICMD="~/bin/start_gpsi.sh xinit $TOTALPEERS $INSTANCES $GOSSIPINTERVAL $LISTENTIME $WAITTIME ~/sig/$DATADIR/ $TEAMSIZE $BANDS $ROWS $COMPRESSION 0 0 0 junk junk 0"

# start XGossip exec
# with killed peers
#GPSICMD="~/bin/start_gpsi.sh xexec $TOTALPEERS $INSTANCES $GOSSIPINTERVAL $LISTENTIME $WAITTIME junk $TEAMSIZE $BANDS $ROWS $COMPRESSION $ROUNDS 0 0 junk $KILLFILE $KILLRANGE"
# with churn (additional peers joining and leaving)
#GPSICMD="~/bin/start_gpsi.sh xexec $TOTALPEERS $INSTANCES $GOSSIPINTERVAL $LISTENTIME $WAITTIME junk $TEAMSIZE $BANDS $ROWS $COMPRESSION $ROUNDS $CHURNPEERS $BOOTPORT $SESSIONSFILE $KILLFILE $KILLRANGE"

# start Broadcast
#GPSICMD="~/bin/start_gpsi.sh bcast $TOTALPEERS $INSTANCES $GOSSIPINTERVAL $LISTENTIME $WAITTIME ~/sig/$DATADIR/ 0 0 0 $COMPRESSION 0"

# init
ACTIVE=0
ALL=0
DO=0
CONFIG=0
DEBUG=0
GET=0
HOST=0
INSTALL=0
PUT=0
COUNT=0
KILL=0
LINE_NUM=0
NUM=0
HOST=0
OTHER=0
PS=0
REMOTEALL=0
REMOTEPROC=0
START=0
TEST=0
n=0
totallsd=0
totalgpsi=0
z=0

cat $ALLHOSTS | grep -v ^# > ${ALLHOSTS}.tmp
TOTALHOSTS=`wc -l < ${ALLHOSTS}.tmp`

if [ $# -lt 2 -o $# -gt 3 ]; then
	echo -e 1>&2 "Usage: $0\t[all | host] [config]"
	echo -e 1>&2 "\t\t\t[all | host] [count]\t[gpsi | lsd | all]"
	echo -e 1>&2 "\t\t\t[all | host] [do]\t[cmd]"
	echo -e 1>&2 "\t\t\t[all | host] [get]\t[gpsi | lsd | init]"
	echo -e 1>&2 "\t\t\t[all | host] [install]"
	echo -e 1>&2 "\t\t\t[all | host] [kill]\t[gpsi | lsd | all]"
	echo -e 1>&2 "\t\t\t[all | host] [ps]\t[gpsi | lsd | all]"
	echo -e 1>&2 "\t\t\t[all | host] [put]\t[gpsi | all | files]"
	echo -e 1>&2 "\t\t\t[all | host] [start]\t[gpsi | lsd]"
	echo -e 1>&2 "\t\t\t[all | host] [test]"
	exit 127
fi

case "$1" in
	(all) ALL=1; NUM=$TOTALHOSTS;;
	(*) OTHER=1;;
esac

case "$2" in
	(kill) KILL=1;;
	(start) START=1;;
	(ps) PS=1;;
	(config) CONFIG=1;;
	(install) INSTALL=1;;
	(put) PUT=1;;
	(count) COUNT=1;;
	(test) TEST=1;;
	(do) DO=1;;
	(get) GET=1;;
	(*) echo "not reached"; exit 127;;
esac

if [ $# -ne 3 -a $GET = 1 ]; then
	echo "missing parameter"
	exit 127
elif [ $# -ne 3 -a $PS = 1 ]; then
	echo "missing parameter"
	exit 127
elif [ $# -ne 3 -a $COUNT = 1 ]; then
	echo "missing parameter"
	exit 127
elif [ $# -ne 3 -a $KILL = 1 ]; then
	echo "missing parameter"
	exit 127
elif [ $# -ne 3 -a $PUT = 1 ]; then
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

if [ $OTHER = 1 ] ; then
	HOST=$1
fi

if [ $START = 1 -a $REMOTEPROC = "bootstrap" ] ; then
	echo "not implementedused"
	#ssh $USER@$HOST $LSDCMD &
else
	if [ $ALL = 1 ] ; then
		LIST=`cat ${ALLHOSTS}.tmp`
	elif [ $OTHER = 1 ] ; then
		LIST=$1
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
				ssh $USER@$i killall -s ABRT lsd adbd syncd gpsi &
			elif [ $REMOTEPROC = "gpsi" ] ; then
				ssh $USER@$i killall -s ABRT gpsi &
			elif [ $REMOTEPROC = "lsd" ] ; then
				ssh $USER@$i killall -s ABRT lsd adbd syncd &
			else
				echo "something is wrong"
			fi
			n=`expr $n + 1`;
		elif [ $START = 1 ] ; then
			echo " (start $REMOTEPROC): "
			if [ $REMOTEPROC = "lsd" ] ; then
				ssh $USER@$i $LSDCMD &
				echo "sleeping for 60 seconds..."
				sleep 60
			elif [ $REMOTEPROC = "gpsi" ] ; then
				ssh $USER@$i $GPSICMD &
			fi
			n=`expr $n + 1`;
		elif [ $PS = 1 ] ; then
			echo " (ps):"
			if [ $REMOTEALL = 1 ] ; then
				ssh $USER@$i ps xw
				ret="-10"
				n=`expr $n + 1`;
			elif [ $REMOTEPROC = "gpsi" ] ; then
				# put 1st char in [] to exclude grep
				ret=`ssh ${USER}@${i} ps xw|grep [g]psi >/dev/null; echo $?`
			elif [ $REMOTEPROC = "lsd" ] ; then
				ret=`ssh ${USER}@${i} ps xw|grep [l]sd >/dev/null; echo $?`
			else
				echo "something is wrong"
			fi
			PSFILE="hosts.ps-$CURRTIME.$REMOTEPROC"
			PSFILEDOWN="hosts.ps-$CURRTIME.$REMOTEPROC.down"
			# if host is responding
			if [ $ret -eq 255 ]; then
				echo "host is down"
			elif [ $ret -eq 0 ]; then
				echo "$REMOTEPROC is running"
				echo $i >> $PSFILE
				n=`expr $n + 1`;
			elif [ $ret -eq 1 ]; then
				echo "$REMOTEPROC is NOT running"
				echo $i >> $PSFILEDOWN
			# if REMOTEALL
			elif [ $ret -eq -10 ]; then
				echo -n ""
			else
				echo "something is wrong"
			fi
		elif [ $PUT = 1 ] ; then
			echo ' (put):'
			if [ $REMOTEALL = 1 ] ; then
				scp $BUILDHOME $USER@$i:~/
				ssh $USER@$i "mkdir ~/bin $REMOTETMP $REMOTETMPL $REMOTETMPG ~/etc ~/sig ~/log"
				scp $SCRIPTSHOME/* $USER@$i:~/bin
				scp $SCRIPTSHOME/.bashrc $USER@$i:~/
				scp $POLYPATH $USER@$i:~/etc/
				scp $DATAHOME $USER@$i:~/sig/
			elif [ $REMOTEPROC = "gpsi" ] ; then
				scp $GPSIBIN $USER@$i:$PSIXHOME/bin/
			else
				#scp ~/bin/cdn/setup_tomcat $USER@$i:~/
				#scp ~/bin/cdn/setup_webserver $USER@$i:~/
				#scp $BUILDHOME $USER@$i:~/
				#ssh $USER@$i "mkdir ~/bin; mkdir ~/tmp; mkdir ~/etc; mkdir ~/sig; mkdir ~/log"
				#scp $SCRIPTSHOME/* $USER@$i:~/bin
				#scp $SCRIPTSHOME/.bashrc $USER@$i:~/
				#scp $SCRIPTSHOME/mv_logs_back.sh $USER@$i:~/bin
				#scp $SCRIPTSHOME/run_peers.sh $USER@$i:~/bin
				#scp $SCRIPTSHOME/join_chord.sh $USER@$i:~/bin
				#scp $SCRIPTSHOME/leave_chord.sh $USER@$i:~/bin
				#scp ~/build/xgossip/psix.new/bin/lsd/start-dhash2 $USER@$i:~/raopr/psix.new/bin/lsd
				#scp $LOCALHOME/tmp/allIDs.log $USER@$i:~/tmp
				#scp $POLYPATH $USER@$i:~/etc/
				#scp $DATAHOME $USER@$i:~/sig/
				#scp $SESSIONSDIR/sessions-$TOTALCHURNPEERS.log.$z $USER@$i:~/etc/sessions-$TOTALCHURNPEERS.log
				#scp $GPSIBIN $USER@$i:$PSIXHOME/bin/
				scp $KILLDIR/killed-$TOTALCHURNPEERS.log.$z $USER@$i:~/etc/killed-$TOTALCHURNPEERS.log
				#scp $SCRIPTSHOME/start_gpsi.sh $USER@$i:~/bin
				z=`expr $z + 1`
			fi
			n=`expr $n + 1`;
		elif [ $INSTALL = 1 ] ; then
			echo ' (install):'
			ssh $USER@$i "mkdir -p raopr; mv psix.new.tar.gz raopr/; cd raopr; tar xfz psix.new.tar.gz; mv psix.new.tar.gz ../; cd ..;"
			ssh $USER@$i "cd ~/sig; tar zxf ~/sig/$DATAFILE"
			n=`expr $n + 1`;
		elif [ $TEST = 1 ] ; then
			echo -n " (test): "
			ssh $USER@$i $GPSIPATH -v
			# generic test
			#ret=`ssh ${USER}@${i} uname >/dev/null; echo $?`
		elif [ $COUNT = 1 ] ; then
			echo -n ' (count): '
			if [ $REMOTEALL = 1 ] ; then
				CMD="ps ax | grep [s]yncd | wc -l"
				lsdcount=`ssh $USER@$i $CMD`
				totallsd=`expr $totallsd + $lsdcount`
				echo -n "lsd: $lsdcount"
				CMD="ps ax | grep [g]psi | wc -l"
				gpsicount=`ssh $USER@$i $CMD`
				totalgpsi=`expr $totalgpsi + $gpsicount`
				echo " gpsi: $gpsicount"
			elif [ $REMOTEPROC = "lsd" ] ; then
				CMD="ps ax | grep [s]yncd | wc -l"
				lsdcount=`ssh $USER@$i $CMD`
				totallsd=`expr $totallsd + $lsdcount`
				echo $lsdcount
			elif [ $REMOTEPROC = "gpsi" ] ; then
				CMD="ps ax | grep [g]psi | wc -l"
				gpsicount=`ssh $USER@$i $CMD`
				totalgpsi=`expr $totalgpsi + $gpsicount`
				echo $gpsicount
			fi
			n=`expr $n + 1`;
		elif [ $DO = 1 ] ; then
			echo ' (do): '
			ssh $USER@$i $CMD &
			#ssh $USER@$i uptime
			#ssh -t -i ~/Desktop/cdn.pem $USER@$i /home/ec2-user/setup_webserver
			#ssh -t -i ~/Desktop/cdn.pem $USER@$i /home/ec2-user/setup_tomcat
			n=`expr $n + 1`;
		elif [ $GET = 1 ] ; then
			echo " (get $REMOTEPROC):"
			logdir="20121229-init-k8l10s8f15a10"
			scp $USER@$i:/media/ephemeral0/$logdir.tgz $LOCALHOME/log/aws/tmp/$i-$logdir.tgz &
			#scp $USER@$i:/media/ephemeral0/tmp.churn.gpsi.tgz $LOCALHOME/log/aws/tmp/$i-tmp.churn.gpsi.tgz &
			#scp $USER@$i:/media/ephemeral0/tmp.churn.lsd.tgz $LOCALHOME/log/aws/tmp/$i-tmp.churn.lsd.tgz &
			#scp $USER@$i:~/log/$logdir.tgz $LOCALHOME/log/aws/tmp/$i-$logdir.tgz &
			#scp $USER@$i:~/instanceIDs.log $LOCALHOME/log/aws/tmp/$i-instanceIDs.log &
			#scp $USER@$i:$PSIXHOME/tmp/lsd-1/log.$REMOTEPROC $LOCALHOME/tmp/log.$i.$REMOTEPROC &
			n=`expr $n + 1`;
		elif [ $CONFIG = 1 ] ; then
			echo ' (config):'
			ssh $USER@$i cat $CONFIGPATH
			n=`expr $n + 1`;
		fi
		#echo -ne "\n"
	done

	echo "$n hosts successful"
	echo "total lsd: $totallsd"
	echo "total gpsi: $totalgpsi"
fi
#set +x

#rm ${ALLHOSTS}.tmp
