#!/bin/sh

#set -x

USER=vsfgd
HOMEDIR="/home/$USER"

SRC="$HOMEDIR/build/chord-static"
XGOSSIPATH="$HOMEDIR/build/xgossip"
DST="$XGOSSIPATH/psix.new/bin"

# compile eth
cd $SRC
make

# remove old binaries
#rm $SRC/tools/psi
rm $SRC/tools/gpsi
rm $SRC/lsd/adbd
rm $SRC/lsd/lsd
rm $SRC/lsd/lsdctl
rm $SRC/lsd/sampled
rm $SRC/lsd/syncd

# compile static binaries
cd $SRC/lsd
$HOMEDIR/bin/run_static.lsd
cd $SRC/tools
#$HOMEDIR/bin/run_static.psi
$HOMEDIR/bin/run_static.gpsi

# copy static binaries
#cp $SRC/tools/psi $DST
cp $SRC/tools/gpsi $DST
cp $SRC/lsd/adbd $DST/lsd
cp $SRC/lsd/lsd $DST/lsd
cp $SRC/lsd/lsdctl $DST/lsd
cp $SRC/lsd/sampled $DST/lsd
cp $SRC/lsd/syncd $DST/lsd

# create package
cd $XGOSSIPATH
rm psix.new.tar.gz
tar czvf psix.new.tar.gz psix.new
