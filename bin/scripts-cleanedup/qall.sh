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
		./query.sh $query $DTDPATH/$i
		n=`expr $n + 1`
		if [ $n -eq 20 ] ; then
			break;
		fi
	done
	n=1
done
