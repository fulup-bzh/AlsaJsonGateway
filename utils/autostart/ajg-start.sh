#!/bin/sh

# Object: Sample of autostart. Check AJG is responding every XX seconds and restart if needed
# Author: Fulup Ar Foll
# Copyright: what ever will make you happy

EMAIL=fulup@breizhme.net
PORT=1234
CHKTIME=20  # check AJG every xxx seconds
DAEMON=ajg-daemon
FAKEMOD=--fakemod    # comment for realmod
VERBOSE=--verbose


BASEDIR=`dirname $0`/..
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
if test -f `dirname $0`/start.conf; then
    echo "using `dirname $0`/start.conf"
    source `dirname $0`/start.conf
fi

if test -z "FAKEMOD"; then
  echo "WARNING: running in fake mod"
fi

# kill any existing daemon
pkill $DAEMON

# start a background instance of AJG
$BINDIR/$DAEMON $VERBOSE $FAKEMOD --port=$PORT --rootdir=$ROOTDIR --sessiondir=$SESSIONDIR --daemon

while true; do
  wget --output-document /tmp/ping.ajg http://localhost:1234/jsonapi?request=ping-get
  if test $? -ne 0; then
     echo "AJG server fail to response [try restart]" | mail -s Restarting AJG on `hostname` $EMAIL
     $BINDIR/$DAEMON $VERBOSE $FAKEMOD --port=$PORT --rootdir=$ROOTDIR --sessiondir=$SESSIONDIR --daemon --restart
  fi
  sleep $CHKTIME
done

