#!/bin/bash
# Praveen Rao.
# UMKC, 2008
if [ $# -ne 2 ];
then
  echo "Usage: run_peers <peers> <bootport>"
  exit 1;
fi

USER=`whoami`
HOME="/home/$USER"
PEERS=${1}
BOOTHOST="10.195.73.135"
BOOTPORT=${2}
PORT=$BOOTPORT
BINDIR="${HOME}/raopr/psix.new/bin"
SRCDIR="${HOME}/raopr/psix.new/bin"

killall -ABRT lsd syncd adbd >& /dev/null

for ((i=1;i<=$PEERS;i++))
do
  rm -rf ${i}

  # every 30 peers, the boot port changes
  #mod=`expr ${i} % 30`
  #if [ $mod -eq 0 ] ; then
     #BOOTPORT=`expr $BOOTPORT + 5`
     #echo "BOOTPORT: $BOOTPORT"
  #fi

  echo "BOOTPORT: $BOOTPORT"
  $SRCDIR/lsd/start-dhash --root ${i} --bindir ${BINDIR} -j $BOOTHOST:$BOOTPORT -p $PORT &
  echo "sleeping..."
  sleep 2
  PORT=`expr $PORT + 5`
done
