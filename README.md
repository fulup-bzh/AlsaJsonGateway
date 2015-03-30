# alsa-json-gateway

---------------------------------------------------
WARNING: Under heavy development, feel free to help
---------------------------------------------------

Alsa-Json-Gateway is a port of amixer.c from Alsa utils to respond in JSON to HTML5 client.
The gateway also provide basic file service CSS/JS fils that UI may need.

Online Client Example: http://breizhme.net/alsa-json-gateway

To build [Linux Only]

    #install dependencies: alsa-dev libmicrohttpd-dev json-c-dev
    cd src; make;

Start server
   ./built/daemon-ajg --help
   ./built/daemon-ajg --verbose --rootdir=$HOME//Documents/WorkSpace/AlsaJsonMixer/wwww

Request in REST directly from browser
   http://localhost:1234/alsa-json?request=get-controls&sndcard=0

Through AlsaJsonMixer HTML5 UI
   http://localhost:1234


