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

# publish 38*$n documents
while [ $n -le 10 ] ; do
	for i in $DTDS; do
		#if [ $n -lt 97 ] ; then
			#continue
		#fi
		echo "DTD: $i"
		echo "XML: $i.$n.xml"
		XML="$XMLPATH/$i/$i.$n.xml"
		if [ ! -f $XML ] ; then
			echo "$XML is missing"
			echo $XML >> files-$CURRTIME.missing
			echo
			continue
		fi
		#echo "0 $XML $DTDPATH/$i"
		# if $1=0, publish.sh uses "date +%s.%N" for DOCID
		./gensign.sh $i $XML $DTDPATH/$i
		#SLEEP=$(( $RANDOM % 20 ))
		#SLEEP=10
		#echo "sleeping for $SLEEP sec..."
		#sleep $SLEEP
	done
	n=`expr $n + 1`
done
