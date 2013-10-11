#!/bin/sh

HOMEDIR="/home/vsfgd"
IPADDR="127.0.0.1"

LOGDIR="$HOMEDIR/log"
LOGFILE="$LOGDIR/publish.log"
RESULTS="$LOGDIR/publish-results.csv"
SIGDIR="$HOMEDIR/tmp"
WORKINGDIR="$HOMEDIR/tmp"

# format
echo "maxreplicas,maxretrieveretries,host,time,xml,dtd,time taken,docs inserted" >> $RESULTS

tr "," " " < $LOGFILE | while read HOST CURRTIME XMLFILE DTDFILE
do
	var=`grep 'MAXREPLICAS' $SIGDIR/$IPADDR.$CURRTIME.publishfile`
	# everything to the right of the rightmost colon
	right=${var##*: }
	echo -n "$right," >> $RESULTS

	var=`grep 'MAXRETRIEVERETRIES' $SIGDIR/$IPADDR.$CURRTIME.publishfile`
	right=${var##*: }
	echo -n "$right," >> $RESULTS

	echo -n "$HOST," >> $RESULTS
	echo -n "$CURRTIME," >> $RESULTS
	echo -n "$XMLFILE," >> $RESULTS
	echo -n "$DTDFILE," >> $RESULTS

	var=`grep "Time taken to process" $SIGDIR/$IPADDR.$CURRTIME.publishfile`
	# everything to the right of the rightmost colon
	right=${var##*: }
	#echo -n "$right," >> $RESULTS
	# everything to the left of the first space
	left=${right%% *}
	echo -n "$left," >> $RESULTS

	var=`grep "documents inserted" $SIGDIR/$IPADDR.$CURRTIME.publishfile`
	right=${var##*: }
	echo -n "$right," >> $RESULTS
	echo >> $RESULTS
done
