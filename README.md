

alsa-json-gateway


Provides an HTTP REST interface to ALSA mixer for HTML5 UI support. The main objective of AJG is to decouple ALSA from UI, this especially for Music Oriented Sound boards like Scarlett Focurite and others.


* Author: Fulup Ar Foll http://breizhme.net/en/author
* Demo:   http://breizhme.net/alsajson/mixers/ajg#/

Alpha-01 release.

   * REST/API and JSON response should remains stable.
   * All described features should work.
   * May still leak resources on some cases [hopefully not much]
   * This is a free software shipped with not guaranty, and you're more than welcome to help.

Supported features

*Supports every ALSA/Mixer controls base on 'amixer' capabilities.
* Config store/Restore of sndcard setting on disk in JSON format.
* Supports HTTP/GET to allow HTML5/JS/CSS download without imposing an extra web server.*
* Runs in foreground or background as a disconnected daemon.
* Does not require administrator privilege.

Building [Linux Only]

    1) download and expend alsa-json-gateway archive from github

    2) install dependencies [should be available in any distributions, eventually under different names]
       a: alsa-dev
       b: libmicrohttpd-dev
       c: json-c-dev

       * Centos/Redhat/Fedora:  sudo yum install json-c-devel libmicrohttpd-devel alsa-lib-devel
       * OpenSuse:              sudo zypper install libjson-c-devel libmicrohttpd-devel alsa-lib-devel
       * Ubuntu/Mint/Debian:    sudo apt-get install json-c.dev libmicrohttpd.dev libasound2-dev

    3) cd src; make install;   # Alpha version does not have installation process.

Starting alsa-json-gateway

      export AJW_DIR=$HOME/AJW; mkdir $AJW_DIR
      ajg-daemon --help                                                        # get options
      ajg-daemon --rootdir=$AJW_DIR --verbose --port=1234                      # run foreground in verbose mode
      ajg-daemon --config=$AJW_DIR/AJG-config.json --rootdir=AJW_DIR  --save   # run save config
      ajg-daemon --config=AJW_DIR/AJG-config.json  --daemon                    # run in background mode
      ajg-daemon --config=AJW_DIR/AJG-config.json  --kill                      # kill current AJG daemon
      ajg-daemon --config=AJW_DIR/AJG-config.json  --fakemod                   # simulate sndcard ignoring set/get control

REST API
     - GENERIC Arguments
           cardid=hw:xxx  xxx=card number [0-31]
           numid=xxx    xxx=control numid [depend on sound boards]
           quiet=0,1,2  0=verbose [default] 1=no enums,acl,000 2=just enough to control sndcard

     - GATEWAY_PING: #! ping AlsaJson gateway   ## amixer -c0 cget numid=first
           http://localhost:1234/jsonapi?request=ping-get
           http://localhost:1234/jsonapi?request=ping-get&cardid=hw:0

     - CARD_GET_ALL: ## aplay -L
           http://localhost:1234/jsonapi?request=card-get-all

     - CARD_GET_ONE:  #! get name and info from cardid=hw:0
           http://localhost:1234/jsonapi?request=card-get-one&cardid=hw:0

     - CTRL_GET_ALL: ## amixer -c0 contents
           http://localhost:1234/jsonapi?request=ctrl-get-all&cardid=hw:0

     - CTRL_GET_ONE: ## amixer -c0 cget numid=3
           http://localhost:1234/jsonapi?request=ctrl-get-one&cardid=hw:0&numid=5&quiet=0

     - CTRL_SET_ONE: ## amixer -c0 cget numid=5 '10,20' Note: setone use ALSA hight level API and support enums as value arguments
           http://localhost:1234/jsonapi?request=ctrl-set-one&cardid=hw:0&quiet=1&numid=5&value=10,5

     - CTRL_SET_MANY: ## set multiple numids at the same value level [usefull for stereo sync setup] only accept integer values
           http://localhost:1234/jsonapi?request=ctrl-set-many&cardid=hw:0&quiet=1&numids=[5,6,7,8,8]&value=[10,5,7]

     - SESSION_STORE: #! store on disk cardid=hw:0 config under name MySoundConfig
           http://localhost:1234/jsonapi?request=session-store&cardid=hw:0&session=MySoundConfig  &info={Optional AJG_info session description object}

     - SESSION_LIST: #! list existing session on disk for cardid=hw:0
           http://localhost:1234/jsonapi?request=session-list&cardid=hw:0

     - SESSION_LOAD: #! upload MySoundConfig session into cardid=hw:0
           http://localhost:1234/jsonapi?request=session-load&cardid=hw:0&session=MySoundConfig

WARNING remarks:

* ctrl setting values change depending on sndcard and numid. Check with CTRL_GET_ALL to find appropriated value for your config.
* load/store of session is per/board using on Alsa sndcard short name. Warning: session store from a given card should not be used on a different model.
* configs and sessions are store in JSON and accessible within Session directory. Nevertheless manually editing should be avoided, and hacking controls value may potentially damage your sound equipment.

Missing Features :

* make clean mine type for CSS, JS, JSON.
* implement websock to support realtime monitoring.
* package inside OBS to provide binary packages.


