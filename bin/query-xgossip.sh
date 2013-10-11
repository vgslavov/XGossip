#!/bin/sh

BOOTSTRAPHOST="hosts.bootstrap"
CURRTIME=`date +%Y%m%d%H%M%S.%N`
DOCID=$CURRTIME
IPADDR="127.0.0.1"
IRRPOLYVAL=190553
MAXREPLICAS=1
# 1=nearest, 0=primary
REPLICATYPE=0
# don't enclose "~" paths in quotes
USER=`whoami`
REMOTEUSER=umkc_rao1
HOMEDIR="/home/$USER"
REMOTEDIR="/home/$REMOTEUSER"
TMPDIR="$HOMEDIR/tmp"
LOGDIR="$HOMEDIR/log/query"
LOGFILE="$LOGDIR/query-xgossip.log"
SIGDIR="$HOMEDIR/tmp"
WORKINGDIR="$HOMEDIR/tmp"
REMOTEINSTALLDIR="/home/$REMOTEUSER/raopr/psix.new"
TMPFILE="$WORKINGDIR/query.tmp"
XPATHDTD="$WORKINGDIR/$IPADDR.$CURRTIME.dtd"
XPATHFILE="$WORKINGDIR/$IPADDR.$CURRTIME.xpath"

# default (multiply polynomials)
#XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator-multi"
# don't multiply irreducable polynomials
#XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator-poly"
# rao's version
#XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator-rao"
# latest
#XMLSIGNGENERATOR="$HOMEDIR/P2PXML/bin/XMLSignGenerator"

# proper way to do OR in if condition
if [ $# -lt 1 -o $# -gt 2 ]; then
	echo -e 1>&2 "Usage: $0 sig-file [host]"
	exit 127
fi

QUERYFILE=`basename $1`
QUERYDIR=`dirname $1`

if [ $# -eq 2 ]; then
	HOST=$2
elif [ -f $BOOTSTRAPHOST ]; then
        HOST=`cat $BOOTSTRAPHOST`
        echo "HOST: $HOST"
else
        echo "no host available"
        exit 127
fi

# compress file
#cd $QUERYDIR; tar cvfz $QUERYFILE.tar.gz $QUERYFILE > /dev/null; mv $QUERYFILE.tar.gz $SIGDIR

# run query
# don't run in background (it will start many psi processes)
ssh $REMOTEUSER@$HOST cat < $QUERYDIR/$QUERYFILE "> $REMOTEDIR/$QUERYFILE; rm -rf $REMOTEDIR/query; mkdir $REMOTEDIR/query; mv $REMOTEDIR/$QUERYFILE $REMOTEDIR/query; $REMOTEINSTALLDIR/bin/gpsi -H -Q -S $REMOTEINSTALLDIR/tmp/lsd-1/dhash-sock -s $REMOTEDIR/query/ -F $REMOTEDIR/hash.dat -d 1122941 -j $REMOTEDIR/irrpoly-deg9.dat -B 10 -R 16 -u" >& $TMPDIR/$QUERYFILE.queryfile

# old
#ssh $REMOTEUSER@$HOST cat < $SIGDIR/$QUERYFILE.tar.gz "> $QUERYFILE.tar.gz ; rm -rf $REMOTEDIR/query; mkdir $REMOTEDIR/query; cd $REMOTEDIR/query; tar xvfz $REMOTEDIR/$QUERYFILE.tar.gz > /dev/null; $REMOTEINSTALLDIR/bin/gpsi -H -Q -S $REMOTEINSTALLDIR/tmp/lsd-1/dhash-sock -s $REMOTEDIR/query/ -F $REMOTEDIR/hash.dat -d 1122941 -j $REMOTEDIR/irrpoly-deg9.dat -B 10 -R 16 -u" >& $SIGDIR/$QUERYFILE.queryfile

# check return status of above command?

#echo $HOST,$CURRTIME,$XPATHQUERY,$DTD >> $LOGFILE

