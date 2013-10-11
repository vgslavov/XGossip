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
	echo "Usage: $0 <files-per-dtd>"
	exit 1;
fi

NUM=$1
n=1

while [ $n -le $NUM ] ; do
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
	done
	n=`expr $n + 1`
done
