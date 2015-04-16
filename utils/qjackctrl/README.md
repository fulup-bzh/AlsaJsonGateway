README: QjaclCtrl.sh

QjackCtrl.sh somehow automates jackd/pulseaudio/ardour cohabitation
It can be started manually or directly from QjacjCtrl GUI from setup/optional/scripts


Operations to verify your configuration
-----------------------------------------

0) Install command in /usr/local/bin
  ./Install-QjackCtrl.sh

1) Find the name of your sound card
   play -l" [default card is hw:0]

2) Make sure no existing jackd server already lock your audio device
 - QjackCtrl.sh  alsadev=hw:0  action=kill    # hard kill on any jackd/jackdbus instance
 - QjackCtrl.sh  alsadev=hw:0  action=check   # sanity check of your environment

3) Start your jacdbus server
 - QjackCtrl.sh  alsadev=hw:0  action=start   # start jackdbus instance 
 - QjackCtrl.sh  alsadev=hw:0  action=connect # connect midi devices and PulseAudio to Jackdbus

4) verify your server is effectively started
- QjackCtrl.sh  alsadev=hw:0  action=status  # check jackdbus running status and locked audio resources

4) Stop your server
 - QjackCtrl.sh  alsadev=hw:0  action=stop    # stop jackdbus and midi bridge

5) Store your config when your happy with your preferences
 - QjackCtrl.sh  alsadev=hw:0  action=config  # store preferences in $HOME/.config/jack


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
     

