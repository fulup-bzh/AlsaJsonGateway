#
# http Makefile
#
#  on most Linux typing "make install" should be enough
# -----------------------------------------------------

VERSION=0.1
DESDIR=/opt/alsa-json-gateway

CCFLAGS=`pkg-config --cflags alsa libmicrohttpd json-c`
LDFLAGS=`pkg-config --libs alsa libmicrohttpd json-c`

CC= gcc -Werror -g -Wstrict-prototypes -Dhttp_VERSION=$(VERSION) -I../include $(CCFLAGS) -DAJQ_VERSION=$(VERSION)
LD= gcc -g $(LDFLAGS)
DATE=`date +%d:%b:%y`
BUILT=../built


OBJECTS= $(BUILT)/main-ajg.o $(BUILT)/config-ajg.o $(BUILT)/httpd-ajg.o $(BUILT)/alsa-ajg.o  $(BUILT)/session-ajq.o

$(BUILT)/%.o: %.c ../include/local-def-ajg.h ../include/proto-def-ajg.h
	$(CC) -c -o $@ $<

all: $(BUILT)/alsajson-gw

$(BUILT)/alsajson-gw: $(OBJECTS)
	$(LD) $(EXTRALIBS) -o $@ $(OBJECTS)

clean:
	$(RM) http $(OBJECTS) core *~

testmem: all
	valgrind $(BUILT)/alsajson-gw --verbose --rootdir=$(HOME)/Documents/WorkSpace/AlsaJsonMixer/www --save

debug: all
	echo --verbose --rootdir=$(HOME)/Documents/WorkSpace/AlsaJsonMixer/www --save
	gdb  $(BUILT)/alsajson-gw

distrib: clean
	cd .. ; rm -f http-$(VERSION); ln -s http-2.x http-$(VERSION)
	cd .. ; tar -czvhf http-$(VERSION).tgz http-$(VERSION)

update: $(DIST)/http
	cp mixer-server $(DESDIR)/bin

install: http
	echo installing http in $(DESDIR)
	mkdir -p    /var/log/http
	mkdir -p    /var/run/http
	mkdir -p    $(DESDIR)/bin
	mkdir -p    $(DESDIR)/etc
	cp http   $(DESDIR)/bin
	@if test -f $(DESDIR)/etc/http.conf; then cp --update $(DESDIR)/etc/http.conf $(DESDIR)/etc/http.sav; fi
	cp --update http.conf $(DESDIR)/etc
	@if test ! -f /etc/http.conf; then ln -sf $(DESDIR)/etc/http.conf /etc; fi
	cp Tools/http.rc /etc/init.d/http
	ln -sf /etc/init.d/http /etc/rc.d/rc5.d/S99http
