#!/bin/sh

#set -x

USER=vsfgd
HOMEDIR="/home/$USER"
SIGDIR="$HOMEDIR/sig-all/all"
SIGLIST=`ls $SIGDIR`
ALLHOSTS="hosts.all.tmp"
HOSTLIST=`cat ${ALLHOSTS}`

n=1

while [ $n -le 10 ] ; do
	for j in $SIGLIST; do
		for i in $HOSTLIST; do
			for k in `ls $SIGDIR/$j`; do
				scp $SIGDIR/$j/$k umkc_rao1@$i:~/sig/
			done
		done
	done
	n=`expr $n + 1`
done
