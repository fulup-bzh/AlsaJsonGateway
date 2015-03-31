# alsa-json-gateway

---------------------------------------------------
WARNING: Under heavy development, feel free to help
---------------------------------------------------

Alsa-Json-Gateway is a port of amixer.c from Alsa utils to respond in JSON to HTML5 client.
The gateway also provide basic file service CSS/JS fils that UI may need.

Online Client Example: http://breizhme.net/alsajson

To build [Linux Only]

    #install dependencies: alsa-dev libmicrohttpd-dev json-c-dev
    cd src; make;

Start server
   ./built/alsajson-gw --help
   ./built/alsajson-gw --verbose --rootdir=$HOME//Documents/WorkSpace/AlsaJsonMixer/wwww

Request in REST directly from browser
   http://localhost:1234/alsajson?request=get-cards                           ## aplay -L
   http://localhost:1234/alsajson?request=get-ctrls&sndcard=0                 ## amixer -c0 contents
   http://localhost:1234/alsajson?request=get-ctrls&sndcard=1&numid=3&quiet=1 ## amixer -c1 cget numid=3
   http://localhost:1234/alsajson?request=set-ctrls&sndcard=1&numid=3&args=1,2 ## amixer -c1 cget numid=3 '10,20'

Through AlsaJsonMixer HTML5 UI
   http://localhost:1234


