#!/bin/sh

if [ $# -ne 1 ]; then
	echo "Usage: $0 <my root dir>"
	exit 1;
fi

#set -x

MYROOT=$1

kill `cat $MYROOT/pid.adbd`
kill `cat $MYROOT/pid.lsd`
kill `cat $MYROOT/pid.syncd`
#rm -rf $MYROOT
