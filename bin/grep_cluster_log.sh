#!/bin/sh

if [ $# -ne 4 ]; then
        echo "Usage: grep_local_log.sh <src> <type> <lines> <pattern>"
        exit 1;
fi

USER=vsfgd
HOME="/home/$USER"
SRC=${1}
TYPE=${2}
LINES=${3}
PATTERN=${4}

for i in `ls $SRC/*.$TYPE` ; do
	#echo $i:
	# last occurence of pattern
	#grep "$PATTERN" $i | tail -1;
	#grep -A 1 -B $LINES "$PATTERN" $i;

	# 1st occurence of pattern
	#grep -m 1 "$PATTERN" $i;

	# num of times pattern occurs
	#grep -c "$PATTERN" $i;

	# most generic: show each line with a pattern match
	grep "$PATTERN" $i;

	# generate teamids tmp file
	#grep -e "teamID:" $i | awk -F : '{print $6}' | sort | uniq >> $SRC/log.teamids.tmp

	# old
	# show last line
	#tail -n 1 $SRC/log.$TYPE;
	# combine teamids
	#cat $SRC/log.teamids >> ~/tmp/log.teamids.tmp
	#wc -l $SRC/log.teamids | awk '{print $1}' >> $SRC/log.teamids
	#grep -e "^teamID:" $i | sort | awk '{print $6}' | uniq > $SRC/log.teamids
	# calc sigs/peer
	#grep listsize $i | awk -F : '{total=total+$4}END{print total}'
	#var=`grep -c -B $LINES "$PATTERN" $SRC/log.$TYPE | tail -1`;
	#total=`expr $total + $var`;
	#grep -B $LINES "$PATTERN" $SRC/log.$TYPE | tail -1;
	#grep -B $LINES "$PATTERN" $SRC/log.$TYPE;
done

#echo $total

#sort $SRC/log.teamids.tmp | uniq > $SRC/log.teamids
#rm $SRC/log.teamids.tmp
