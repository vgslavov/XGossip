#!/bin/sh

CURRTIME=`date +%Y%m%d%H%M%S`
USER=vsfgd
HOMEDIR="/home/$USER"
DATADIR="$HOMEDIR/P2PXML"
DTDPATH="$DATADIR/DTDs"
XMLPATH="$DATADIR/data"
# because DTDs > XMLs
DTDS=`cd $XMLPATH;ls -d *.dtd`

if [ $# -ne 1 ]; then
	echo "Usage: $0 <distro>"
	exit 1;
fi

DISTRO="$1"
DISTROPATH="$XMLPATH/$DISTRO"

echo "DTD: $DISTRO"
for i in `ls $DISTROPATH`; do
	echo "XML: $i"
	XML="$DISTROPATH/$i"
	./gensign.sh $DISTRO $XML $DTDPATH/treebank.dtd
done
