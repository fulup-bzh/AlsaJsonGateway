#!/bin/sh

# Syntax ./Install-QjackCtrtl.sh
#
# Nota: should be be started as root or sudo user
#       command will ask for admin user is needed
#
# Object:
#    - install QjackCtrtl.sh into /usr/local/bin
#    - grant realtime priority to audio group
#    - rename jackd in order to force jackdbus usage
# --------------------------------------------------

DIRNAME=`dirname $0`

if test `id -u` -eq 0; then
  echo "HOOPS script SHOULD-NOT be started as root. Please retry as a normal [$SUDO_USER] user"
  exit 1
fi

# check jackdbus is intalled
JACKDBUS=`which jackdbus`
if test -x "$JACKDBUS"; then
   echo "OK JACKDBUS found at $JACKDBUS"
else
   echo "HOOPS: JACKDBUS not found MUST install"
   exit 1
fi

# Check user as access to audio group
id | grep "audio" >/dev/null
if test $? -ne 0; then
    echo "Add USER=$LOGNAME to audio group"  
    sudo -p "[%p] password:" usermod -a -G audio $LOGNAME
else 
  echo "OK USER=$LOGNAME part of [audio]"
fi

# Check user as realtime access
LIMIT=`ulimit`
if test "$LIMIT" != "unlimited"; then

    # check no previous install was done
    echo "Add [Realtime] to audio group"
    grep QjackCtrl /etc/security/limits.conf >/dev/null
    if test $? -eq 0; then
      echo "Hoops: you need to logout/login for audio realtime to be activated"
    else
      sudo -p "[%p] password:" cp /etc/security/limits.conf /etc/security/limits.conf.ori
      sudo rm -f /tmp/limits.conf
      cp /etc/security/limits.conf /tmp
      echo "# grant realtime to audio group QjackCtrl/Jack" >>/tmp/limits.conf
      echo "@audio   -  rtprio     70"         >> /tmp/limits.conf
      echo "@audio   -  memlock    unlimited"  >> /tmp/limits.conf
      echo "@audio   -  nice      -19"         >> /tmp/limits.conf
      echo "# ---- end QjackCtrl/Jack ----"       >> /tmp/limits.conf
      sudo mv /tmp/limits.conf /etc/security/limits.conf
      sudo chown root /etc/security/limits.conf

      echo "you have to logout in order realtime access for audio group to be activated"
   fi
else
  echo "OK USER=$LOGNAME get real-time access" 
fi
    
# Remove exec bit to jackd in order no one to use it
JACKCMD=`which jackd 2>/dev/null`
if test "$JACKCMD" != ""; then
  if test -x "$JACKCMD" ; then
    echo "Force rename of $JACKCMD to jackd.ori"
    sudo -p "[%p] password:" mv $JACKCMD $JACKCMD.ori
  fi
fi

mkdir -p ~/.log/jack

# make QjackCtrtl.sh available globally
if test -f $DIRNAME/QjackCtrl.sh; then
  echo "Installing QjackCtrl.sh into /usr/local/bin"
  sudo -p "[%p] password:" cp $DIRNAME/QjackCtrl.sh /usr/local/bin/.
  sudo -p "[%p] password:" chmod a+rx /usr/local/bin/QjackCtrl.sh
else
  echo "Hoop: where is QjackCtrl.sh !!!"
  echo "Not a valid install package in: $DIRNAME"
  exit 1
fi

if test -x /usr/local/bin/QjackCtrl.sh; then
  echo "QjackCtrl.sh installation OK"
else
  echo "something went wrong QjackCtrl.sh not installed"
fi

