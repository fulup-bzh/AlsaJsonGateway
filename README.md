alsa-json-gateway

Object: provides an HTTP REST interface to ALSA mixer for HTML5 UI support

Author: Fulup Ar Foll http://breizhme.net/en/author

Demo:   http://breizhme.net/alsajson

Alpha-01 release.

   * API should now remains stable.
   * All described features should work.
   * May still leak resources on some cases [hopefully not much]
   * This is a free software shipped with not guaranty.

Supported features

   Supports all ALSA/Mixer controls base on 'amixer' capabilities.
   Config store/Restore of sndcard setting on disk in JSON format.
   Supports HTTP/GET to allow HTML5/JS/CSS download without imposing an extra web server.
   Runs in foreground or background as a disconnected daemon.
   Does not require administrator privilege.

Building [Linux Only]

    #download and expend alsa-json-gateway archive from github
    #install dependencies [should be available in any distributions, eventually under different names]
       1: alsa-dev
       2: libmicrohttpd-dev
       3: json-c-dev

    cd src; make;   # Alpha version does not have installation process.

Starting alsa-json-gateway

      export AJW_DIR=$HOME/AJW; mkdir -p $AJW_DIR/sessions
      ./built/alsajson-gw --help                                                        # get options
      ./built/alsajson-gw --rootdir=$AJW_DIR --verbose --port=1234                      # run foreground in verbose mode
      ./built/alsajson-gw --config=$AJW_DIR/AJG-config.json --rootdir=AJW_DIR  --save   # run save config
      ./built/alsajson-gw --config=AJW_DIR/AJG-config.json  --daemon                    # run in background mode
      ./built/alsajson-gw --config=AJW_DIR/AJG-config.json  --kill                      # kill current AJG daemon

REST API
     - GATEWAY_PING: #! ping AlsaJson gateway   ## amixer -c0 cget numid=first
           http://localhost:1234/jsonapi?request=ping-get
           http://localhost:1234/jsonapi?request=ping-get&sndcard=0

     - CARD_GET_ALL: ## aplay -L
           http://localhost:1234/jsonapi?request=card-get-all

     - CARD_GET_ONE:  #! get name and info from sndcard=0
           http://localhost:1234/jsonapi?request=card-get-one&sndcard=0

     - CTRL_GET_ALL: ## amixer -c0 contents
           http://localhost:1234/jsonapi?request=ctrl-get-all&sndcard=0

     - CTRL_GET_ONE: ## amixer -c0 cget numid=3
           http://localhost:1234/jsonapi?request=ctrl-get-one&sndcard=0&numid=5&quiet=0

     - CTRL_SET_ONE: ## amixer -c0 cget numid=5 '10,20'
           http://localhost:1234/jsonapi?request=ctrl-set-one&sndcard=0&quiet=1&numid=5&args=10,5

     - SESSION_STORE: #! store on disk sndcard=0 config under name MySoundConfig
           http://localhost:1234/jsonapi?request=session-store&sndcard=0&args=MySoundConfig

     - SESSION_LIST: #! list existing session on disk for sndcard=0
           http://localhost:1234/jsonapi?request=session-list&sndcard=0

     - SESSION_LOAD: #! upload MySoundConfig session into sndcard=0
           http://localhost:1234/jsonapi?request=session-load&sndcard=0&args=MySoundConfig

WARNING remarks:

* ctrl setting values change depending on sndcard and numid. Check with CTRL_GET_ALL to find appropriated value for your config.
* load/store of session is per/board using on Alsa sndcard short name. Warning: session store from a given card should not be used on a different model.
* configs and sessions are store in JSON and accessible within Session directory. Nevertheless manually editing should be avoided, and hacking controls value may potentially damage your sound equipment.

Missing Features :

* make clean mine type for CSS, JS, JSON.
* implement websock to support realtime monitoring.
* package inside OBS to provide binary packages.


