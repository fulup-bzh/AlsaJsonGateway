#
# http Makefile
#
#  on Linux typing "make install" should be enough
# -----------------------------------------------------

VERSION=0.1

ifndef DESTDIR
	DESTDIR:=/opt/ajg-daemon
endif


CCFLAGS=`pkg-config --cflags alsa libmicrohttpd json-c`
LDFLAGS=`pkg-config --libs alsa libmicrohttpd json-c`

CC= gcc -Werror -g -Wstrict-prototypes -Dhttp_VERSION=$(VERSION) -I../include $(CCFLAGS) -DAJQ_VERSION=$(VERSION)
LD= gcc -g $(LDFLAGS)
DATE=`date +%d:%b:%y`
BUILT=../built


OBJECTS= $(BUILT)/main-ajg.o $(BUILT)/config-ajg.o $(BUILT)/httpd-ajg.o $(BUILT)/alsa-ajg.o  $(BUILT)/session-ajq.o

$(BUILT)/%.o: %.c ../include/local-def-ajg.h ../include/proto-def-ajg.h
	$(CC) -c -o $@ $<

all: $(BUILT)/ajg-daemon

$(BUILT)/ajg-daemon: $(OBJECTS)
	$(LD) $(EXTRALIBS) -o $@ $(OBJECTS)

clean:
	$(RM) http $(OBJECTS) core *~

testmem: all
	valgrind $(BUILT)/ajg-daemon --verbose --rootdir=$(HOME)/Documents/WorkSpace/AlsaJsonMixer/www --save

debug: all
	echo --verbose --rootdir=$(HOME)/Documents/WorkSpace/AlsaJsonMixer/www --save
	gdb  $(BUILT)/ajg-daemon

distrib: clean
	cd .. ; rm -f http-$(VERSION); ln -s http-2.x http-$(VERSION)
	cd .. ; tar -czvhf http-$(VERSION).tgz http-$(VERSION)

built: all
	mkdir -p $(DESTDIR)
	@mkdir -p $(DESTDIR)/bin
	@mkdir -p $(DESTDIR)/www
	@install --strip $(BUILT)/ajg-daemon $(DESTDIR)/bin

	@echo ========================================================
	@echo  AJG install OK [check with ajg-daemon --help]
	@echo ========================================================

install: built
	@(cd /usr/local/bin; ln -fs $(DESTDIR)/bin/* .)

update: $(DIST)/http
	cp mixer-server $(DESTDIR)/bin

