#!/bin/sh

# Object: Sample of autostart. Check AJG is responding every XX seconds and restart if needed
# Author: Fulup Ar Foll
# Copyright: what ever will make you happy

EMAIL=fulup@xxxxxxx
PORT=1234
CHKTIME=600  # check AJG every xxx seconds
DAEMON=ajg-daemon
HOSTNAME=`hostname`

# Comment unwanted options
FAKEMOD="--fakemod"
VERBOSE="--verbose"
OPTIONS="$FAKEMOD $VERBOSE"


BASEDIR=`dirname $0`/../..
cd $BASEDIR;  BASEDIR=`pwd`
echo BASEDIR=$BASEDIR


# check if we are in dev mod with AlsaMixer in the same parent directory
if test -d $BASEDIR/../AlsaJsonMixer/www; then
 BINDIR=$BASEDIR/built

 ROOTDIR=$BASEDIR/../AlsaJsonMixer/www
 cd $ROOTDIR; ROOTDIR=`pwd`
 SESSIONDIR=$ROOTDIR/sessions

else
 BINDIR="/opt/ajg-daemon/bin"  # Global installation in /opt
 ROOTDIR="/opt/ajg-daemon/www"
 SESSIONDIR="$HOME/.ajg-daemon"

 # create session dir and check AJG-mixer is installed
 mkdir -p $SESSIONDIR

 if test ! -f "$ROOTDIR/index.html"; then
   echo "WARNING: ROOTDIR=$ROOTDIR is does not hold INDEX.html"
 fi

fi

# Overload default with config file
if test -f $SESSIONDIR/AJG-autostart.conf; then
    echo "using $SESSIONDIR/AJG-autostart.conf"
    source $SESSIONDIR/AJG-autostart.conf
fi

if test -z "FAKEMOD"; then
  echo "WARNING: running in fake mod"
fi

# kill any existing daemon
pkill $DAEMON

# start a background instance of AJG
$BINDIR/$DAEMON $OPTIONS --port=$PORT --rootdir=$ROOTDIR --sessiondir=$SESSIONDIR --daemon

while true; do
  wget --quiet --output-document /tmp/ping.ajg http://localhost:$PORT/jsonapi?request=ping-get
  if test $? -ne 0; then
     echo "AJG server $HOSTNAME fail to response [try restart]" | mail -s "Restarting AJG on $HOSTNAME" $EMAIL
     $BINDIR/$DAEMON $OPTIONS --port=$PORT --rootdir=$ROOTDIR --sessiondir=$SESSIONDIR --daemon --restart
  fi
  sleep $CHKTIME
done

