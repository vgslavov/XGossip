#!/bin/sh

DATADIR='/home/raopr/cs5590ld/P2PXML'
DTDPATH="$DATADIR/DTDs"
XMLPATH="$DATADIR/data"
# because DTDs > XMLs
DTDS=`cd $XMLPATH;ls -d *.dtd`
XPATH="$DATADIR/XPathQueries"

n=1

for i in $DTDS; do
   	cat $XPATH/$i/$i.qry | while read query; do
		echo "DTD: $i"
		echo "XPath query: $query"
		#echo "$query $DTDPATH/$i"
		./query.sh $query $DTDPATH/$i
		#SLEEP=$(( $RANDOM % 61 ))
		#SLEEP=10
		#echo "sleeping for $SLEEP sec..."
		#echo
		#sleep $SLEEP
		n=`expr $n + 1`
		if [ $n -eq 20 ] ; then
			break;
		fi
	done
	n=1
done
