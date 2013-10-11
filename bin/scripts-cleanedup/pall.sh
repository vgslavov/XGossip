#!/bin/sh

DATADIR="/home/raopr/cs5590ld/P2PXML"
HOMEDIR="/home/vsfgd"
CURRTIME=`date +%Y%m%d%H%M%S`

DTDPATH="$DATADIR/DTDs"
XMLPATH="$DATADIR/data"
# because DTDs > XMLs
DTDS=`cd $XMLPATH;ls -d *.dtd`
XPATH="$DATADIR/XPathQueries"
LOGDIR="$HOMEDIR/log"
WORKINGDIR="$HOMEDIR/tmp"

n=1

while [ $n -le 100 ] ; do
	for i in $DTDS; do
		if [ $n -lt 97 ] ; then
			continue
		fi
		echo "DTD: $i"
		echo "XML: $i.$n.xml"
		XML="$XMLPATH/$i/$i.$n.xml"
		if [ ! -f $XML ] ; then
			echo "$XML is missing"
			echo $XML >> files-$CURRTIME.missing
			echo
			continue
		fi
		# if $1=0, publish.sh uses "date +%s.%N" for DOCID
		./publish.sh 0 $XML $DTDPATH/$i
	done
	n=`expr $n + 1`
done
