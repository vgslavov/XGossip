#!/bin/sh

CURRTIME=`date +%Y%m%d%H%M%S.%N`
DOCID=$CURRTIME
IPADDR="127.0.0.1"
IRRPOLYVAL=190553
# don't enclose "~" paths in quotes
USER=vsfgd
HOMEDIR="/home/$USER"
TMPDIR="$HOMEDIR/tmp"
LOGDIR="$HOMEDIR/log"
LOGFILE="$LOGDIR/publish.log"
SIGDIR="$HOMEDIR/tmp"
WORKINGDIR="$HOMEDIR/tmp"
TMPFILE="$WORKINGDIR/query.tmp"
XPATHDTD="$WORKINGDIR/$IPADDR.$CURRTIME.dtd"
XPATHFILE="$WORKINGDIR/$IPADDR.$CURRTIME.xpath"

# default (multiply polynomials)
#XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator-multi"
# don't multiply irreducable polynomials
XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator-poly"
# rao's version
#XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator-rao"
# latest
#XMLSIGNGENERATOR="$HOMEDIR/P2PXML/bin/XMLSignGenerator"

# proper way to do OR in if condition
if [ $# -lt 1 -o $# -gt 2 ]; then
	echo -e 1>&2 "Usage: $0 xpath-query [dtdfile]"
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

if [ $# -eq 2 ]; then
	#XPATHDTD="$DISTRODIR/$NAME.dtd"
	cp $2 $XPATHDTD
fi

#export LD_LIBRARY_PATH='/home/raopr/bdb/lib:/home/raopr/bdbxml/lib:/usr/lib'

# create a signature
if [ $# -eq 2 ]; then
	# to avoid getting 0-sized sigs, set 3rd arg to 2 instead of 0
	#$XMLSIGNGENERATOR $XMLFILE $XPATHDTD 2 $IRRPOLYVAL $SIGFILE 1 $DOCID >& $ERRFILE
	$XMLSIGNGENERATOR $XPATHFILE $XPATHDTD 1 $IRRPOLYVAL $SIGDIR/sig.$IPADDR.$CURRTIME 2 >& $SIGDIR/$IPADDR.$CURRTIME.errorfile
else
	#$XMLSIGNGENERATOR $XMLFILE junk 2 $IRRPOLYVAL $SIGFILE 5 $DOCID >& $ERRFILE
	$XMLSIGNGENERATOR $XPATHFILE junk 1 $IRRPOLYVAL $SIGDIR/sig.$IPADDR.$CURRTIME 6 >& $SIGDIR/$IPADDR.$CURRTIME.errorfile
fi

# check return status of above command

# compress signatures
cd $SIGDIR; tar cvfz sig.$IPADDR.$CURRTIME.tar.gz sig.$IPADDR.$CURRTIME* > /dev/null; cd - >/dev/null

# check return status of above command?
