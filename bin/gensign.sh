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

# default (multiply polynomials)
#XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator-multi"
# don't multiply irreducable polynomials
XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator-poly"
# rao's version
#XMLSIGNGENERATOR="$HOMEDIR/bin/XMLSignGenerator-rao"
# latest
#XMLSIGNGENERATOR="$HOMEDIR/P2PXML/bin/XMLSignGenerator"

# proper way to do OR in if condition
if [ $# -lt 2 -o $# -gt 3 ]; then
	echo -e 1>&2 "Usage: $0 <distro> <xmlfile> [dtdfile]"
	exit 127
fi

DISTRO=$1
DISTRODIR=$TMPDIR/$DISTRO

if [ ! -d $DISTRODIR ] ; then
	mkdir $DISTRODIR
fi

# includes commented hosts (?)
NAME=`basename $2 .xml`
XMLFILE="$DISTRODIR/$NAME.xml"
ERRFILE="$DISTRODIR/$NAME.err"
SIGFILE="$DISTRODIR/$NAME.sig"

cp $2 $XMLFILE

if [ $# -eq 3 ]; then
	XMLDTD="$DISTRODIR/$NAME.dtd"
	cp $3 $XMLDTD
fi

#export LD_LIBRARY_PATH='/home/raopr/bdb/lib:/home/raopr/bdbxml/lib:/usr/lib'

# create a signature
if [ $# -eq 3 ]; then
	# to avoid getting 0-sized sigs, set 3rd arg to 2 instead of 0
	$XMLSIGNGENERATOR $XMLFILE $XMLDTD 2 $IRRPOLYVAL $SIGFILE 1 $DOCID >& $ERRFILE
else
	$XMLSIGNGENERATOR $XMLFILE junk 2 $IRRPOLYVAL $SIGFILE 5 $DOCID >& $ERRFILE
fi

# clean up DISTRODIR
rm -rf $DISTRODIR/*.dtd
rm -rf $DISTRODIR/*.err
rm -rf $DISTRODIR/*.xml
rm -rf $DISTRODIR/*.hist
rm -rf $DISTRODIR/*.int
rm -rf $DISTRODIR/*.tags
rm -rf $DISTRODIR/*.text
rm -rf $DISTRODIR/*.texthash
#rm -rf $DISTRODIR/*.publishfile
#rm -rf $DISTRODIR/*.sig*
