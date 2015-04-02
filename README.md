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
   http://localhost:1234/jsonapi?request=ping-get                                     #! ping AlsaJson gateway
   http://localhost:1234/jsonapi?request=ping-get&sndcard=0                           ## amixer -c0 cget numid=first
   http://localhost:1234/jsonapi?request=card-get-all                                 ## aplay -L
   http://localhost:1234/jsonapi?request=card-get-one&sndcard=0                       ## get name and info from sndcard=0
   http://localhost:1234/jsonapi?request=ctrl-get-all&sndcard=0                       ## amixer -c0 contents
   http://localhost:1234/jsonapi?request=ctrl-get-one&sndcard=0&numid=5&quiet=1       ## amixer -c1 cget numid=3
   http://localhost:1234/jsonapi?request=ctrl-set-one&sndcard=1&numid=3&args=1,2      ## amixer -c1 cget numid=3 '10,20'
   http://localhost:1234/jsonapi?request=session-list                                 #! list all stored session config
   http://localhost:1234/jsonapi?request=session-load&sndcard=0&args=sessionname      #! restore Alsa mixer config from xxxx session
   http://localhost:1234/jsonapi?request=session-store&sndcard=0&args=sessionname     #! store current Alsa mixer config in xxxx

Through AlsaJsonMixer HTML5 UI
   http://localhost:1234


TBD:
   - insert a alsajson element in request
   - implement set/get session by reading/writing from disk to board.
   - make mine type for CSS, JS, JSON
   - handle error signal with auto restart
   - check for background mode
   - implement setuid
   - implement console output


