#!/bin/sh

if [ $# -ne 4 ]; then
        echo "Usage: grep_local_log.sh <peers> <type> <lines> <pattern>"
        exit 1;
fi

USER=`whoami`
HOME="/home/$USER"
TMP="$HOME/tmp"
PEERS=${1}
TYPE=${2}
LINES=${3}
PATTERN=${4}

# 0-based
#PEERS=`expr $PEERS - 1`
while [ $PEERS -gt 0 ] ; do
	echo "$PEERS:";
	#var=`grep -c -B $LINES "$PATTERN" $TMP/$PEERS/log.$TYPE | tail -1`;
	#total=`expr $total + $var`;
	grep -B $LINES "$PATTERN" $TMP/$PEERS/log.$TYPE | tail -1;
	#grep -B $LINES "$PATTERN" $TMP/$PEERS/log.$TYPE;

	# generate teamids file
	#grep -e "^teamID:" $TMP/$PEERS/log.$TYPE | sort | awk '{print $2}' | uniq > $TMP/$PEERS/log.teamids
	##wc -l $TMP/$PEERS/log.teamids | awk '{print $1}' >> $TMP/$PEERS/log.teamids

	# show last line
	#tail -n 1 $TMP/$PEERS/log.$TYPE;

	# combine teamids
	#cat $TMP/$PEERS/log.teamids >> ~/tmp/log.teamids.tmp

        PEERS=`expr $PEERS - 1`
done

#echo $total

#sort ~/tmp/log.teamids.tmp | uniq > ~/tmp/log.teamids
#rm ~/tmp/log.teamids.tmp
