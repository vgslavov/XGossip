#!/bin/sh

CURRTIME=`date +%Y%m%d%H%M%S`
USER=vsfgd
HOMEDIR="/home/$USER"
DATADIR="$HOMEDIR/P2PXML"
DTDPATH="$DATADIR/DTDs"
XMLPATH="$DATADIR/data"
# because DTDs > XMLs
DTDS=`cd $XMLPATH;ls -d *.dtd`

for i in $DTDS; do
	XMLS=`cd $XMLPATH/$i;ls`
	for j in $XMLS; do
		echo "DTD: $i"
		echo "XML: $j"
		./gensign.sh $i $XMLPATH/$i/$j $DTDPATH/$i
	done
done
