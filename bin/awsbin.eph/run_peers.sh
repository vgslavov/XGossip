#!/bin/bash
# Praveen Rao.
# UMKC, 2008
if [ $# -ne 4 ];
then
  echo "Usage: run_peers <peers> <boot port> <start port> <churn: 0|1>"
  exit 1;
fi

USER="ec2-user"
HOME="/home/$USER"
PEERS=${1}
BOOTPORT=${2}
STARTPORT=${3}
CHURN=${4}
BOOTHOST="10.6.149.130"
BINDIR="${HOME}/raopr/psix.new/bin"
SRCDIR="${HOME}/raopr/psix.new/bin"

#killall -ABRT lsd syncd adbd >& /dev/null

if [ $CHURN -eq 1 ] ; then
  $SRCDIR/lsd/start-dhash2 --root ./ --bindir ${BINDIR} -j $BOOTHOST:$BOOTPORT -p $STARTPORT &
  exit;
fi

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
  $SRCDIR/lsd/start-dhash --root ${i} --bindir ${BINDIR} -j $BOOTHOST:$BOOTPORT -p $STARTPORT &
  echo "sleeping..."
  sleep 2
  STARTPORT=`expr $STARTPORT + 5`
done
