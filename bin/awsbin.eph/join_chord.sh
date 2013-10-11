#!/bin/sh

if [ $# -ne 3 ]; then
	echo "Usage: $0 <boot port> <my port> <my root dir>"
	exit 1;
fi

#set -x

BOOTPORT=$1
MYPORT=$2
MYROOT=$3
USER="ec2-user"
HOME="/home/$USER"
SCRIPTS="$HOME/bin"

#mkdir $MYROOT
cd $MYROOT; $SCRIPTS/run_peers.sh 1 $BOOTPORT $MYPORT 1
