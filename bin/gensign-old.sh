#!/bin/sh

CURRTIME=`date +%Y%m%d%H%M%S.%N`
#CURRTIME=`date +%s.%N`
IPADDR="127.0.0.1"
IRRPOLYVAL=190553
# don't enclose "~" paths in quotes
HOMEDIR="/home/vsfgd"
SIGDIR="$HOMEDIR/tmp"
LOGDIR="$HOMEDIR/log"
LOGFILE="$LOGDIR/publish.log"
XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator"

DISTRO=$1
DISTRODIR=$SIGDIR/$DISTRO

if [ ! -d $DISTRODIR ] ; then
	mkdir $DISTRODIR
fi

# includes commented hosts
XMLDTD="$DISTRODIR/$IPADDR.$CURRTIME.dtd"
XMLFILE="$DISTRODIR/$IPADDR.$CURRTIME.xml"

#set -x

# proper way to do OR in if condition
if [ $# -lt 2 -o $# -gt 3 ]; then
	echo -e 1>&2 "Usage: $0 distro xmlfile [dtdfile]"
	exit 127
fi

#set -x


DOCID=$CURRTIME

cp $2 $XMLFILE
XML=`basename $2`
DTD=''

if [ $# -eq 3 ]; then
	cp $3 $XMLDTD
	DTD=`basename $3`
fi

export LD_LIBRARY_PATH='/home/raopr/bdb/lib:/home/raopr/bdbxml/lib:/usr/lib'

#set -x

# create a signature
if [ $# -eq 3 ]; then
	$XMLSIGNGENERATOR $XMLFILE $XMLDTD 0 $IRRPOLYVAL $DISTRODIR/sig.$IPADDR.$CURRTIME 1 $DOCID >& $DISTRODIR/$IPADDR.$CURRTIME.errorfile
else
	$XMLSIGNGENERATOR $XMLFILE junk 0 $IRRPOLYVAL $DISTRODIR/sig.$IPADDR.$CURRTIME 5 $DOCID >& $DISTRODIR/$IPADDR.$CURRTIME.errorfile
fi

# clean up DISTRODIR
rm -rf $DISTRODIR/*$IPADDR*.dtd
rm -rf $DISTRODIR/*$IPADDR*.errorfile
rm -rf $DISTRODIR/*$IPADDR*.xml
rm -rf $DISTRODIR/sig.$IPADDR*.hist
rm -rf $DISTRODIR/sig.$IPADDR*.int
rm -rf $DISTRODIR/sig.$IPADDR*.tags
rm -rf $DISTRODIR/sig.$IPADDR*.text
rm -rf $DISTRODIR/sig.$IPADDR*.texthash
#rm -rf $DISTRODIR/*$IPADDR*.publishfile
#rm -rf $DISTRODIR/sig.$IPADDR*
