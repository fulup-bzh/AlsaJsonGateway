#!/bin/sh
# Object of QjackCtrl.sh
#    This script somehow automates jackd/pulseaudio/ardour cohabitation
#    It can be started my hand or directly from Qjacjcrl setup/optional/scripts
# 
#    Author:    Fulup Ar Foll
#    Date  :    26-May-2014
#    Version:   1.3
#    Copyright: What ever will make you happy :)
# 
# Syntax
#     path/QjackCtrl.sh action=start|stop|help  
#
# Documentation
#
#    Start defining your ALSADEVICE [line 54]
#
#    Typical Qjack startup scripts should be
#     - "before start" QjackCtrl.sh  action=clean
#     - "after start"  QjackCtrl.sh  action=start
#     - "after stop"   QjackCtrl.sh  action=stop
#
#    When used manually
#     - QjackCtrl.sh  action=check   # sanity check of your environment
#     - QjackCtrl.sh  action=clean   # any jackd/dbus server with "servername=xxxx"
#     - QjackCtrl.sh  action=start   # start jackdbus instance 
#     - QjackCtrl.sh  action=connect # connect midi devices and pulseaudio to jackd
#     - QjackCtrl.sh  action=stop    # stop jackdbus and midi bridge
#     - QjackCtrl.sh  action=status  # check jackdbus running status and locked audio resources
#     - QjackCtrl.sh  action=kill    # hard kill on any jackd/jackdbus instance
#     - QjackCtrl.sh  action=restart # kill jackd/jackdbus servname=xxx instance and restart service
#     - QjackCtrl.sh  action=log     # display jackdbus log file
#     - QjackCtrl.sh  action=config  # store preferences in $HOME/.config/jack
#
#    When testing you can force variable value in commande line
#     - QjackCtrl.sh  alsadev=hw:4 servername=snoopy action=check
# Bug
#       When jackd/dbus servername!="default" most jackd client bugs. The only working solution
#       seems to set $JACK_DEFAULT_SERVER=new-server-name within user global environment level.
#
# Note
#     does not have predefined value for qjackdus but you can force it by hand
#     Do not forget to update limits.conf file for audio group
#     Check jacks socket presence in /dev/shm/jack*
#     To get some useful Ardour3 plugin install "ladspa" package.
#     pacmd list-sinks  ; list available output at pulse
#
# ******************************************************************************************
# Default variable can be override at command line with servername=xxxxx
# ******************************************************************************************
#   https://wiki.archlinux.org/index.php/JACK_Audio_Connection_Kit
# 
servername=default     ;# "using anything else than default will make your life more complex $JACK_DEFAULT_SERVER"
alsadev=hw:0           ;# "can be found with aplay -L"
alsaname=""            ;# "can be found with cat /proc/asound/cards
alsarate=44100         ;# "44100 48000 96000"
jackrate=3             ;# "should be 3 for USB and 2 for PCI"
jackperiod=1024        ;# "should be 64,128,256,1024,2048,4096"
pulsedebug=1           ;# "PulseAudio Log level {4} mini when debuging"
rtkitgroup=audio       ;# "This group must have realtime access in /etc/security/limits.conf"
# ********************************************************************************************

if test -d /etc/sysconfig; then
   CONFDIR=/etc/sysconfig
fi
if test -d /etc/default; then
   CONFIG=/etc/default
fi

if test -f $CONFDIR/QjackCtrlSH.conf; then
   source $CONFDIR/QjackCtrlSH.conf
fi

mkdir -p $HOME/.config/jack
if test -f $HOME/.config/jack/QjackCtrlSH.conf; then
  source $HOME/.config/jack/QjackCtrlSH.conf
fi

# Eval arguments, remove '-' char is prefix set
EvalArgs()
{

  for ARG in "$@"
  do
    arg=`echo $ARG | tr -d -`
    if expr 'index' "$arg" '=' '>' '1' >/dev/null
    then
      case "$prefix" in
      1)
         eval "${NAME}_${arg}"
         if test $? != 0
         then
            echo "ERROR: ${NAME}_${arg} contains special character [$@]"
         fi
        ;;
      2)
         eval "${PREFIX}${NAME}_${arg}"
         if test $? != 0
         then
            echo "ERROR: ${NAME}_${arg} contains special character [$@]"
         fi
        ;;
      *)
        eval "${ARG}"
        if test $? != 0
        then
           echo "ERROR: ${arg} contains special character [$@]"
        fi
        ;;
      esac
    fi
  done
  prefix=0
}

CheckActiveAudioService () 
{
    # check which servers use jack socket
    if test -e /dev/shm/jack_${servername}_${USER_UID}_0; then
      PIDS=`lsof -t /dev/shm/jack_${servername}_${USER_UID}_0`
      if test "$PIDS" != ""; then
        echo ""; echo "$* Processes active on Jack share memory /dev/shm/jack_${servername}_${USER_UID}_0"
      fi
      for PID in $PIDS; do
         NAME=`cat /proc/$PID/cmdline`
         echo " ---> pid=$PID cmd=$NAME"
      done
    echo ""
    fi
}

# Parse command line for parameters
EvalArgs "$@"
if test "$servername" = ""; then
  servername="default"
else
  export JACK_DEFAULT_SERVER="$servername"
fi
USER_UID=`id -u`

# if card was given by name find coresponding cardid
if ! test -z "$alsaname"; then
 SNDCARD=`cat /proc/asound/cards | grep  '\[.*\]' | grep -i $alsaname`
 SNDID=`echo $SNDCARD | awk '{print $1}'`
 alsadev=hw:$SNDID
fi


case "$action" in

check) 
    # this is not a complete test suite, but if something does not work, you MUST fix it before anything else
    echo "***** Start some basic tests for jackdbus *****"

    # stop any jackd running server
    echo "kill any old jackd server"
    jack_control stop
    pkill -9 jackd
    pkill -9 jackdbus

    # Check is jackd is present
    JACKD2_BIN=` which jackd 2>/dev/null`
    if test -x "$JACKD2_BIN"; then
       echo "   WARNING: $JACKD2_BIN executable !!!!"
       echo "   Remove jackd to force jackdbus usage. Command: sudo mv $JACKD2_BIN $JACKD2_BIN.ori"
       echo ""
    fi

    echo "check jackdbus can start manually"
    jack_control ds alsa
    jack_control dps device    $alsadev
    jack_control eps name      $servername
    jack_control start
    jack_control status
    if test $? != 0; then 
       echo "***** ERROR: fail to start [jackdbus auto]"
       exit 1
    fi

    jack_control stop
    jack_control status
    if test $? != 1; then
       echo "***** ERROR: fail to stop [jackdbus auto]"
    fi

    CheckActiveAudioService "Warning: "

    echo "check we have audio group access"
    id | grep "($rtkitgroup)"
    if test $? -ne 0; then
       echo "***** ERROR: user $LOGNAME does not have access to [audio] group  use usermod -a -G $rtkitgroup $LOGNAME"
       exit 1
    fi

    echo "check realtime priority ulimit"
    LIMIT=`ulimit`
    if test "$LIMIT" != "unlimited"; then
      echo "***** ERROR: user $LOGNAME does not access to REALTIME unlimited resource, please check /etc/security/limits.conf"
      exit 1
    fi
    
    echo "check device=$alsadev is present"
    amixer -D $alsadev controls >/dev/null
    if test $? -ne 0; then
       echo "***** ERROR: device [$alsadev] does not exist. Please check ALSA device list with [aplay -l]"
       exit 1
    fi 

    echo "check jack servername"
    if test "$servername" != "default"; then
       echo "WARNING: Jack servername!= [default] you MUST set JACK_DEFAULT_SERVER=$servername in your global env"
       exit 1
    fi

    echo "***** Jackdbus config OK *******"
    ;;

clean)
    if test -e /dev/shm/jack_${servername}_${USER_UID}_0
    then
       PIDS=`lsof -t /dev/shm/jack_${servername}_${USER_UID}_0`
       if test "$PIDS" =  ""; then
            echo "Clean OK: nothing actif on /dev/shm/jack_${servername}_${USER_UID}_0"
       else
         for PID in "$PIDS"; do
           NAME=`readlink /proc/$PID/exe`
           case "`basename $NAME`" in
           jackdbus)
              echo "Clean OK: $NAME already active"
              ;;
           pulseaudio)
              echo "Clean WARN: $NAME connected on /dev/shm/jack [should be OK]"
              ;;
           "")
              echo "Clean HOOPS !!!  /proc/$PID/exe is not a valid link"
              ;;
           *)
              echo "Clean ERROR: $NAME should not lock /dev/shm/jack_${servername}_${USER_UID}_0" 
              echo " Killing ---> pid=$PID cmd=$NAME"
              kill -9 $PID
              ;;
           esac
        done
      fi
    fi
    jack_control status
    ;;

start)
    # start jackdbus
    jack_control eps realtime true
    jack_control ds alsa
    jack_control dps device    $alsadev
    jack_control dps rate      $alsarate
    jack_control dps nperiods  $jackrate
    jack_control dps period    $jackperiod
    jack_control eps name      $servername
    jack_control start

    jack_control status
    if test $? -ne 0; then
       echo "ERROR fail to start JackDBUS"
       STATUS=ERROR
    fi

    # check if jackdbus successfully started
    if test -e /dev/shm/jack_${servername}_${USER_UID}_0; then
      JACKPID=`lsof -t /dev/shm/jack_${servername}_${USER_UID}_0`
      if test "$JACKPID" = ""; then
          STATUS=ERROR  
      fi
    fi

    if test "$STATUS" = "ERROR"; then
         echo "***** ERROR: starting JackDBUS servername=$servername alsadev=$alsadev"
         # Check audio device exist
         amixer -D "$alsadev" controls >/dev/null
       if test $? != 0; then
          echo "*************************************************************"
          echo "    HOOPS !!! alsadev=$alsadev not found [Device oneline ?]"
          echo "              QjackCtrl alsadev=hw:???? action=start"
          echo "*************************************************************"
          exit 1
      else
          echo "HOOP start failed "
          $0 action=status
          echo ""
          echo "**********************************************************"
          echo "Try a QjackCtrl.sh action=status | kill | log | check"
          echo "**********************************************************"
      fi
    fi
    ;;

connect)
    # check jackdbus is running
    jack_control status
    if test $? -ne 0; then
       shift; $0 action=start $*
    fi

    # Make MIDI device visible to JACK
    MIDI_DEV=`amidi --list-devices | grep 'hw:'`
    if test $? -eq 0; then
       a2jmidid -e -j $servername &> ~/.log/jack/a2jmidid.log &
    fi

    # Force PulseAudio Log 
    pacmd  set-log-target file:$HOME/.log/jack/pulseaudio
    pacmd  set-log-level  $pulsedebug
     
    # With jackdbus module-jack-detect should do the job
    # If your PulseAudio fails to automatically activate jack-sink comment those lines
    pactl load-module module-jack-sink   channels=2 connect=yes 2>/dev/null
    pactl load-module module-jack-source channels=2 connect=yes 2>/dev/null

    # Make Jack default device for Pulse
    pacmd set-default-sink jack_out
    pacmd set-default-source jack_in

    ;;
 
stop)
    # If your system fail to unload jack-sink uncomment following lines
    pactl unload-module module-jack-sink 2>/dev/null
    pactl load-module module-jack-source 2>/dev/null

    # Stop jackdbus if running
    pkill a2jmidid
    jack_control status
    if test $? -eq 0; then
       jack_control stop
    fi
    
    CheckActiveAudioService "HOOPS!!! "
    ;;

status)
    # provide jackdbus state and locked sound resource by any other audio server
    echo ""; echo -n "JackdDBUS "
    jack_control status

    echo ""; echo "Active lock on Audio devices ?"
    dbus-send --session --print-reply --reply-timeout=2000 --type=method_call --dest=org.freedesktop.DBus \
              /org/freedesktop/DBus org.freedesktop.DBus.ListNames|grep org.freedesktop.ReserveDevice

    # check which servers use jack socket
    CheckActiveAudioService "Note: "

    # check processes [fuser -v /dev/snd/*]
    echo "Processes locking ALSA sound card"
    for CARD in /dev/snd/pcm*; do
      PIDS=`lsof -t $CARD`
      if test "$PIDS" != ""; then
        for PID in $PIDS; do
           NAME=`cat /proc/$PID/cmdline`
           CARDID=`echo $CARD | sed 's/^.*pcm//' | sed 's/D.*$//'`
           CARDNAME=`ls -l -q  /dev/snd/by-id/* | grep $CARDID | awk '{print $(NF-2)}' |  awk -F '/' '{print $(NF)}'`
           echo " ---> pid=$PID cmd=$NAME SOUNDCARD=$CARD"
           echo "      cardid=$CARDID cardname=$CARDNAME"
        done
      fi
    done
    echo ""
    
    ;;

log)
    # QjackLog should be configured inside setup/options
    echo "LOG from $HOME/.log/jack/jackdbus.log"
    tail $HOME/.log/jack/jackdbus.log
    ;;

kill)
    echo "hard kill all existing jackdbus and jackd instances"
    pkill a2jmidid
    pkill -9 jackd
    pkill -9 jackdbus

    # check remaining process using jack socket
    if test -e /dev/shm/jack_${servername}_${USER_UID}_0; then
  
      PIDS=`lsof -t /dev/shm/jack_${servername}_${USER_UID}_0`
      if test "$PIDS" != ""; then
         echo ""; echo "*** HOOPs processes listening /dev/shm/jack_${servername}_${USER_UID}_0"
         echo "please stop them and retry QjackCtrl.sh action=kill"
      fi
      for PID in $PIDS; do
         NAME=`cat /proc/$PID/cmdline`
         echo " ---> pid=$PID cmd=$NAME"
      done
    fi
    ;;

restart)
   # stop and start jackdbus forcing any other jackd to stop
   echo "restarting jackdbus [servername=$servername] service"
   $0 clean
   $0 stop
   $0 start
   ;;


list)
   cat /proc/asound/cards | grep -e '\[.*\]'
   ;;

config)
   # write user preference in $/home/config/jack/QjackCtrlSH.conf
 
   CONFIG=$HOME/.config/jack/QjackCtrlSH.conf
   echo "Writing config preferences in $CONFIG"
   echo "# QjackCtrlSH config file" > $CONFIG
   date "+# DATE: %m/%d/%y TIME: %H:%M:%S" >>$CONFIG
   echo "#--------------------------------" >>$CONFIG
   echo "servername=$servername    # change it only if you know what you do"      >>$CONFIG
   echo "alsadev=$alsadev          # check devices with: aplay -L"                >>$CONFIG
   echo "alsarate=$alsarate        # 44100 48000 96000"                           >>$CONFIG
   echo "jackrate=$jackrate        # should be 3 for USB and 2 for PCI"           >>$CONFIG
   echo "jackperiod=$jackperiod    # should be 64,128,256,1024,2048,4096"         >>$CONFIG
   echo "pulsedebug=$pulsedebug    # PulseAudio Log level 1-4 mini when debuging" >>$CONFIG
   echo "rtkitgroup=$rtkitgroup    # Must be in /etc/security/limits.conf"        >>$CONFIG
   echo "alsaname=$alsaname        # Devicename has precedence on alsadev"        >>$CONFIG
   ;;

 *)
        echo "Usage: $0 action=check|clean|start|connect|stop|status|log|kill|config"
        exit 1
        ;;
esac
exit 0
