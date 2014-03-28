#!/bin/sh

# distribute the signatures of each dtd among a specific number of peers
if [ $# -ne 4 ]; then
	echo "Usage: $0 <src> <dst> <hosts> <hosts-per-dtd>"
	exit 1;
fi

SRC=$1
DST=$2
HOSTS=$3
PERDTD=$4

#PERDISTRO=$4
#PERHOST=$5
#TOTALSIGS=`expr $PERHOST \* $HOSTS`

n=1
while [ $n -le $HOSTS ] ; do
	mkdir $DST/$n
	n=`expr $n + 1`
done

#set -x

echo "HOSTS: $HOSTS"
echo "HOSTSPERDTD: $PERDTD"
echo "DTD: FILESINDTD, FILESPERHOST"
n=0
z=0
HOSTNUM=1
TOTALSIGS=0
# i is a distro dir
for i in `ls $SRC` ; do
	FILESINDTD=`ls $SRC/$i | wc -l`
	TOTALSIGS=`expr $TOTALSIGS + $FILESINDTD`
	echo -n "$i: $FILESINDTD"
	FILESPERHOST=`expr $FILESINDTD / $PERDTD`
	echo " $FILESPERHOST"
	# j is a sig file
	for j in `ls $SRC/$i` ; do
		if [ $n -lt $FILESPERHOST ] ; then
			#cp $j $DST/$HOSTNUM/
			cp $SRC/$i/$j $DST/$HOSTNUM/
			n=`expr $n + 1`
		else
			HOSTNUM=`expr $HOSTNUM + 1`
			if [ $HOSTNUM -gt $HOSTS ] ; then
				HOSTNUM=1
			fi
			#cp $j $DST/$HOSTNUM/
			cp $SRC/$i/$j $DST/$HOSTNUM/
			n=1
		fi

		z=`expr $z + 1`
		#if [ $z -eq $TOTALSIGS ] ; then
		#	exit
		#fi
	done
done

echo "ACTUALSIGS: $TOTALSIGS"
echo "COPIEDSIGS: $z"

echo "HOSTNUM: # of sigs, # of DTDs"
z=0
sumdtds=0
for i in `ls $DST | sort -n` ; do
	n=`ls $DST/$i | wc -l`
	dtds=`ls $DST/$i |awk -F . '{print $1}'|sort|uniq|wc -l`
	sumdtds=`expr $sumdtds + $dtds`
	echo "$i: $n, $dtds"
	z=`expr $z + $n`
done

echo "TOTALSIGS (sum of SIGSPERHOST): $z"
echo "AVGDTDS/HOST: `expr $sumdtds \/ $HOSTS`"

#if [ $z -ne $TOTALSIGS ] ; then
#	echo "out of signatures"
#fi
