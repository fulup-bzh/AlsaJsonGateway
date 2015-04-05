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

   * /built/alsajson-gw --help
   * /built/alsajson-gw --verbose --rootdir=$HOME//Documents/WorkSpace/AlsaJsonMixer/wwww

Request in REST directly from browser

     - GATEWAY_PING: #! ping AlsaJson gateway   ## amixer -c0 cget numid=first
           http://localhost:1234/jsonapi?request=ping-get
           http://localhost:1234/jsonapi?request=ping-get&sndcard=0

     - CARD_GET_ALL: ## aplay -L
           http://localhost:1234/jsonapi?request=card-get-all

     - CARD_GET_ONE:  #! get name and info from sndcard=0
           http://localhost:1234/jsonapi?request=card-get-one&sendcard=0

     - CTRL_GET_ALL: ## amixer -c0 contents
           http://localhost:1234/jsonapi?request=ctrl-get-all&sndcard=0

     - CTRL_GET_ONE: ## amixer -c1 cget numid=3
           http://localhost:1234/jsonapi?request=ctrl-get-one&sndcard=2&numid=5&quiet=0

     - CTRL_SET_ONE: ## amixer -c1 cget numid=3 '10,20'
           http://localhost:1234/jsonapi?request=ctrl-set-one&sndcard=2&quiet=1&numid=128&args=10,5

     - SESSION_LIST: #! list existing session on disk for a a given card
           http://localhost:1234/jsonapi?request=session-list&sndcard=0

     - SESSION_LOAD: #! upload session from disk into sndcard config
           http://localhost:1234/jsonapi?request=session-load&sndcard=2&args=sessionname

     - SESSION_STORE: #! store on disk current sndcard config
           http://localhost:1234/jsonapi?request=session-store&sndcard=2&args=sessionname


Through AlsaJsonMixer HTML5 UI

   http://localhost:1234


TBD:

   - make mine type for CSS, JS, JSON
   - implement websock to support realtime monitoring


