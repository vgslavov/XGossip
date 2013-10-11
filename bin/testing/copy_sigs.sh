#!/bin/sh

#set -x

USER=vsfgd
HOMEDIR="/home/$USER"
SIGDIR="$HOMEDIR/sig"
SIGLIST=`ls $SIGDIR`
ALLHOSTS="hosts.all.tmp"
HOSTLIST=`cat ${ALLHOSTS}`

for i in $HOSTLIST; do
	for j in $SIGLIST; do
		for k in `ls $SIGDIR/$j`; do
			scp $SIGDIR/$j/$k umkc_rao1@$i:~/sig/
		done
	done
done
