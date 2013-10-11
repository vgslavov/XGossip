#!/bin/sh

if [ $# -ne 1 ]; then
        echo "Usage: $0 <dir>"
        exit 1;
fi

DIR=$1
hosts=`ls $DIR|wc -l`

echo "HOSTNUM: # of sigs, # of DTDs"
z=0
sumdtds=0
for i in `ls $DIR | sort -n` ; do
        n=`ls $DIR/$i | wc -l`
        dtds=`ls $DIR/$i |awk -F . '{print $1}'|sort|uniq|wc -l`
	sumdtds=`expr $sumdtds + $dtds`
        echo "$i: $n, $dtds"
        z=`expr $z + $n`
done

echo "TOTALSIGS (sum of SIGSPERHOST): $z"
echo "AVGDTDS/HOST: `expr $sumdtds \/ $hosts`"
