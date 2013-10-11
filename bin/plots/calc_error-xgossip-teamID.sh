#!/bin/bash

if [ $# -ne 3 ]; then
	echo "Usage: $0 [reports dir] [all teamIDs] [output log]"
	exit 1;
fi

USER=`whoami`
#HOMEDIR="/Users/$USER"
HOMEDIR="/home/$USER"
TMPFILE="$HOMEDIR/tmp/`basename $0`"
REPORTSDIR=${1}
ALLTEAMIDSLOG=${2}
OUTPUTLOG=${3}
# scale for bc
SCL=6

if [ -f $TMPFILE ] ; then
	rm $TMPFILE.*
fi

#set -x

echo -n "teamID," > $TMPFILE.hdr.csv

n=0
for i in `ls $REPORTSDIR`; do
	echo $i
	echo -n "rel. error (%) ($i),sigs ($i),found ($i)" >> $TMPFILE.hdr.csv

	for tid in `cat $ALLTEAMIDSLOG`; do
		npeers=`grep -r -l $tid $REPORTSDIR/$i | wc -l | awk '{print $1}'`
		peers=`grep -r -l $tid $REPORTSDIR/$i`

		echo $tid
		#echo $npeers
		sumerror=0
		avgerror=0
		sigstring=""
		foundstring=""
		for peer in $peers; do
			set -- `grep -m 1 $tid $peer | awk -F "," '{print $2, $3, $5}'`
			found=$1
			relerror=$2
			sigs=$3
			sumerror=`echo "scale=$SCL; $sumerror + $relerror" | bc`
			avgerror=`echo "scale=$SCL; $sumerror / $npeers" | bc`
			sigstring="$sigs;$sigstring"
			foundstring="$found;$foundstring"
		done
		echo -n "$avgerror,$sigstring,$foundstring," >> $TMPFILE.p$i.csv
		echo >> $TMPFILE.p$i.csv
	done
	n=`expr $n + 1`
done
echo >> $TMPFILE.hdr.csv

paste -d "," $ALLTEAMIDSLOG `ls $TMPFILE.p*` >> $OUTPUTLOG
cat $OUTPUTLOG >> $TMPFILE.hdr.csv
mv $TMPFILE.hdr.csv $OUTPUTLOG
