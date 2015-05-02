README: QjaclCtrl.sh

QjackCtrl.sh somehow automates jackd/pulseaudio/ardour cohabitation
It can be started manually or directly from QjacjCtrl GUI from setup/optional/scripts

Vidéo d'explication: in French
 http://www.dailymotion.com/video/x1x3zb3_linux-pulse-jack-scarlette-18i8_tech


Operations to verify your configuration
-----------------------------------------

0) Install command in /usr/local/bin
  ./Install-QjackCtrl.sh

1) Find the name of your sound card
   ./QjackCtrl.sh action=list

2) Make sure no existing jackd server already lock your audio device
 - QjackCtrl.sh  alsadev=hw:USB  action=kill    # hard kill on any jackd/jackdbus instance
 - QjackCtrl.sh  alsaname=scarlett  action=check   # sanity check of your environment

3) Start your jacdbus server
 - QjackCtrl.sh  alsadev=hw:USB  action=start   # start jackdbus instance 
 - QjackCtrl.sh  alsaname=scarlett  action=connect # connect midi devices and PulseAudio to Jackdbus

4) verify your server is effectively started
- QjackCtrl.sh  alsaname=intel  action=status  # check jackdbus running status and locked audio resources

4) Stop your server
 - QjackCtrl.sh  alsadev=hw:USB  action=stop    # stop jackdbus and midi bridge

5) Store your config when your happy with your preferences
 - QjackCtrl.sh  alsadev=hw:USB  action=config  # store preferences in $HOME/.config/jack

Note: alsaname is a selector on your card descriptor and not a real device ID.
Because ALSA change device ID when you plug/unplug USB boards it is almost impossible to track devices by cardid.
You should choose an "alsaname" that is strong enough to find your board in any situation.
For exemple "alsaname=USB" may work one day, but not the other. On the other hand
'alsadev=yamaha' if you have only one yamaha sound board should be fine even on long run.
alsaname has precedence on alsadev. A new alsadev will be recomputed dynamically each time you
restart QjackCtrl.sh


Configure script inside QjackCtrl GUI 
---------------------------------------
  "before start" QjackCtrl.sh  action=clean
  "after start"  QjackCtrl.sh  action=start
  "before stop"  leavec empty
  "after stop"   QjackCtrl.sh  action=stop

Check your final config
------------------------
 - Check log with:   tail -f ~/.log/jeck/QjaclCtrl.log
 - Verify with:      QjackCtrl.sh action=status
 - When jack is started your should see a new device "Jack-Sink" selectable by pulseaudio.
 - When jack is stop pulse should return to your previous output audio device.
 - Tune your jackdbus parameter inside $HOME/.config/jack
     

