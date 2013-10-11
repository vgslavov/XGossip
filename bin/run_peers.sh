#!/bin/bash
# Praveen Rao.
# UMKC, 2008
if [ $# -ne 4 ];
then
  echo "Usage: run_peers <peers> <boot port> <start port> <churn: 0|1>"
  exit 1;
fi

#set -x

USER="vsfgd"
HOME="/home/$USER"
PEERS=${1}
BOOTPORT=${2}
STARTPORT=${3}
CHURN=${4}
BOOTHOST="134.193.130.78"
BINDIR="${HOME}/build/chord"
SRCDIR="${HOME}/src/chord-0.1-vasil"

#killall -ABRT lsd syncd adbd >& /dev/null

if [ $CHURN -eq 1 ] ; then
  $SRCDIR/lsd/start-dhash2 --root ./ --bindir ${BINDIR} -j $BOOTHOST:$BOOTPORT -p $STARTPORT &
  exit;
fi

for ((i=1;i<=$PEERS;i++))
do
  rm -rf ${i}
  $SRCDIR/lsd/start-dhash --root ${i} --bindir ${BINDIR} -j $BOOTHOST:$BOOTPORT -p $STARTPORT &
  STARTPORT=`expr $STARTPORT + 5`
done
