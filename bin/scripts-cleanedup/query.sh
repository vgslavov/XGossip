#!/bin/sh

ACTIVEHOSTS="hosts.active"
ALLHOSTS="hosts.all"
BOOTSTRAPHOST="hosts.bootstrap"
CURRTIME=`date +%Y%m%d%H%M%S.%N`
USER="umkc_rao1"
IPADDR="127.0.0.1"
IRRPOLYVAL=190553
MAXREPLICAS=1
# 1=nearest, 0=primary
REPLICATYPE=0
HOMEDIR="/home/vsfgd"

LOGDIR="$HOMEDIR/log"
LOGFILE="$LOGDIR/query.log"
RESULTS="$LOGDIR/query-results.csv"
SIGDIR="$HOMEDIR/tmp"
WORKINGDIR="$HOMEDIR/tmp"
XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator"

REMOTEINSTALLDIR="/home/$USER/raopr/psix.new"
TOTALHOSTS=`wc -l < ${ALLHOSTS}`
XPATHDTD="$WORKINGDIR/$IPADDR.$CURRTIME.dtd"
XPATHFILE="$WORKINGDIR/$IPADDR.$CURRTIME.xpath"

if [ $# -lt 1 -o $# -gt 2 ]; then
	echo -e 1>&2 "Usage: $0 xpath-query [dtdfile]"
        exit 127
fi

if [ -f $BOOTSTRAPHOST ]; then
        HOST=`cat $BOOTSTRAPHOST`
        echo "HOST: $HOST"
elif [ -f $ACTIVEHOSTS ]; then
	cat $ACTIVEHOSTS | grep -v ^# > ${ACTIVEHOSTS}.tmp
        TOTALACTIVEHOSTS=`wc -l < ${ACTIVEHOSTS}.tmp`
        LINE_NUM=$(( $RANDOM % $TOTALACTIVEHOSTS + 1 ))
	HOST=`sed -n "${LINE_NUM}p" ${ACTIVEHOSTS}.tmp`
	echo -n "$LINE_NUM: "
        echo "HOST: $HOST"
	rm ${ACTIVEHOSTS}.tmp
else
        echo "no active hosts"
        exit 127
fi

# clean up from previous run
rm -rf $WORKINGDIR/*$IPADDR*.dtd
#rm -rf $WORKINGDIR/*$IPADDR*.errorfile
#rm -rf $WORKINGDIR/*$IPADDR*.queryfile
rm -rf $WORKINGDIR/*$IPADDR*.xpath
rm -rf $WORKINGDIR/sig.$IPADDR*

XPATHQUERY=$1
echo $XPATHQUERY > $XPATHFILE
DTD=''

if [ $# -eq 2 ]; then
	cp $2 $XPATHDTD
	DTD=`basename $2`
fi

export LD_LIBRARY_PATH='/home/raopr/bdb/lib:/home/raopr/bdbxml/lib:/usr/lib'

#set -x

# create a signature
if [ $# -eq 2 ]; then
	$XMLSIGNGENERATOR $XPATHFILE $XPATHDTD 1 $IRRPOLYVAL $SIGDIR/sig.$IPADDR.$CURRTIME 2 >& $SIGDIR/$IPADDR.$CURRTIME.errorfile
else
	$XMLSIGNGENERATOR $XPATHFILE junk 1 $IRRPOLYVAL $SIGDIR/sig.$IPADDR.$CURRTIME 6 >& $SIGDIR/$IPADDR.$CURRTIME.errorfile 
fi

# compress signatures
cd $SIGDIR; tar cvfz sig.$IPADDR.$CURRTIME.tar.gz sig.$IPADDR.$CURRTIME* > /dev/null; cd - >/dev/null

echo "querying..."
echo

# run query
# don't run in background (it will start many psi processes)
ssh $USER@$HOST cat < $SIGDIR/sig.$IPADDR.$CURRTIME.tar.gz "> $REMOTEINSTALLDIR/sig.$IPADDR.$CURRTIME.tar.gz ; cd $REMOTEINSTALLDIR; tar xvfz sig.$IPADDR.$CURRTIME.tar.gz > /dev/null; $REMOTEINSTALLDIR/bin/psi $REMOTEINSTALLDIR/tmp/lsd-1/dhash-sock -O $REMOTEINSTALLDIR/sig.$IPADDR.$CURRTIME 0 1 0 1 $REPLICATYPE $MAXREPLICAS" >& $SIGDIR/$IPADDR.$CURRTIME.queryfile

echo $HOST,$CURRTIME,$XPATHQUERY,$DTD >> $LOGFILE

# $RESULTS format:
# maxreplicas,maxretrieveretries,host,time,xpath query,dtd,query time,num docs,hops,dht lookups,docids

var=`grep 'MAXREPLICAS' $SIGDIR/$IPADDR.$CURRTIME.queryfile`
# everything to the left of the rightmost colon
left=${var%:*}
# everything to the right of the rightmost colon
right=${var##*: }
echo -n "$right," >> $RESULTS

var=`grep 'MAXRETRIEVERETRIES' $SIGDIR/$IPADDR.$CURRTIME.queryfile`
right=${var##*: }
echo -n "$right," >> $RESULTS

echo -n "$HOST," >> $RESULTS
echo -n "$CURRTIME," >> $RESULTS
echo -n "$XPATHQUERY," >> $RESULTS
echo -n "$DTD," >> $RESULTS

var=`grep 'Query time' $SIGDIR/$IPADDR.$CURRTIME.queryfile`
right=${var##*: }
left=${right%% *}
echo -n "$left," >> $RESULTS

var=`grep 'Num docs' $SIGDIR/$IPADDR.$CURRTIME.queryfile`
right=${var##*: }
echo -n "$right," >> $RESULTS

var=`grep 'HOPS' $SIGDIR/$IPADDR.$CURRTIME.queryfile`
right=${var##*: }
echo -n "$right," >> $RESULTS

var=`grep 'DHT lookups' $SIGDIR/$IPADDR.$CURRTIME.queryfile`
right=${var##*: }
echo -n "$right" >> $RESULTS

echo >> $RESULTS
