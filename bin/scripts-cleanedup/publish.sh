#!/bin/sh

ACTIVEHOSTS="hosts.active"
ALLHOSTS="hosts.all"
BOOTSTRAPHOST="hosts.bootstrap"
CURRTIME=`date +%Y%m%d%H%M%S.%N`
USER="umkc_rao1"
IPADDR="127.0.0.1"
IRRPOLYVAL=190553
MAXREPLICAS=1
HOMEDIR="/home/vsfgd"
SIGDIR="$HOMEDIR/tmp"
WORKINGDIR="$HOMEDIR/tmp"
LOGDIR="$HOMEDIR/log"
LOGFILE="$LOGDIR/publish.log"
XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator"

REMOTEINSTALLDIR="/home/$USER/raopr/psix.new"
# includes commented hosts
TOTALHOSTS=`wc -l < ${ALLHOSTS}`
XMLDTD="$WORKINGDIR/$IPADDR.$CURRTIME.dtd"
XMLFILE="$WORKINGDIR/$IPADDR.$CURRTIME.xml"

ALL=0

if [ $# -lt 2 -o $# -gt 3 ]; then
	#echo -e 1>&2 "Usage: $0 docid xmlfile [dtdfile]"
	echo -e 1>&2 "Usage: $0 host/0 xmlfile [dtdfile]"
	exit 127
fi

# publish to user-supplied host
if [ $1 != 0 ]; then
	HOST=$1
	echo "HOST: $HOST"
# publish to bootstrap host
elif [ -f $BOOTSTRAPHOST ]; then
	HOST=`cat $BOOTSTRAPHOST`
	echo "HOST: $HOST"
# publish to a random active host
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

# clean up WORKINGDIR
rm -rf $WORKINGDIR/*$IPADDR*.dtd
#rm -rf $WORKINGDIR/*$IPADDR*.errorfile
#rm -rf $WORKINGDIR/*$IPADDR*.publishfile
rm -rf $WORKINGDIR/*$IPADDR*.xml
rm -rf $WORKINGDIR/sig.$IPADDR*

DOCID=$CURRTIME

cp $2 $XMLFILE
XML=`basename $2`
DTD=''

if [ $# -eq 3 ]; then
	cp $3 $XMLDTD
	DTD=`basename $3`
fi

export LD_LIBRARY_PATH='/home/raopr/bdb/lib:/home/raopr/bdbxml/lib:/usr/lib'

# create a signature
if [ $# -eq 3 ]; then
	$XMLSIGNGENERATOR $XMLFILE $XMLDTD 0 $IRRPOLYVAL $SIGDIR/sig.$IPADDR.$CURRTIME 1 $DOCID >& $SIGDIR/$IPADDR.$CURRTIME.errorfile
else
	$XMLSIGNGENERATOR $XMLFILE junk 0 $IRRPOLYVAL $SIGDIR/sig.$IPADDR.$CURRTIME 5 $DOCID >& $SIGDIR/$IPADDR.$CURRTIME.errorfile
fi

# compress signatures
cd $SIGDIR; tar cvfz sig.$IPADDR.$CURRTIME.tar.gz sig.$IPADDR.$CURRTIME* > /dev/null; cd - >/dev/null

echo "publishing..."
echo

# publish the document
# don't run in background (it will start many psi processes)
ssh $USER@$HOST cat < $SIGDIR/sig.$IPADDR.$CURRTIME.tar.gz "> $REMOTEINSTALLDIR/sig.$IPADDR.$CURRTIME.tar.gz ; cd $REMOTEINSTALLDIR; tar xvfz sig.$IPADDR.$CURRTIME.tar.gz > /dev/null; $REMOTEINSTALLDIR/bin/psi $REMOTEINSTALLDIR/tmp/lsd-1/dhash-sock -IS $REMOTEINSTALLDIR/sig.$IPADDR.$CURRTIME 0 1 0 1 0 $MAXREPLICAS" >& $SIGDIR/$IPADDR.$CURRTIME.publishfile

echo $HOST,$CURRTIME,$XML,$DTD >> $LOGFILE
