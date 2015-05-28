#
# http Makefile
#
#  on Linux typing "make install" should be enough
# -----------------------------------------------------

VERSION=0.2.2
AJGDIR=/opt/ajg-daemon

CCFLAGS=`pkg-config --cflags alsa libmicrohttpd json-c`
LDFLAGS=`pkg-config --libs alsa libmicrohttpd json-c`

CC= gcc $(RPM_OPT_FLAGS) -Werror -Wstrict-prototypes -I../include $(CCFLAGS) -DAJQ_VERSION=\"$(VERSION)\"
LD= gcc $(RPM_OPT_FLAGS)
DATE=`date +%d:%b:%y`
BUILT=../built
SAMPL=../samples


OBJECTS= $(BUILT)/main-ajg.o $(BUILT)/config-ajg.o $(BUILT)/httpd-ajg.o $(BUILT)/alsa-ajg.o  $(BUILT)/session-ajq.o

$(BUILT)/%.o: %.c ../include/local-def-ajg.h ../include/proto-def-ajg.h
	$(CC) -c -o $@ $<

all: $(BUILT)/ajg-daemon

$(BUILT)/ajg-daemon: $(OBJECTS)
	$(LD) $(EXTRALIBS) -o $@ $(OBJECTS) $(LDFLAGS)

clean:
	$(RM) http $(OBJECTS) core *~

testmem: all
	valgrind $(BUILT)/ajg-daemon --verbose --rootdir=$(HOME)/Documents/WorkSpace/AlsaJsonMixer/www --save

debug: all
	echo --verbose --rootdir=$(HOME)/Documents/WorkSpace/AlsaJsonMixer/www --save
	gdb  $(BUILT)/ajg-daemon

build: all
	mkdir -p  $(DESTDIR)$(AJGDIR)
	@mkdir -p $(DESTDIR)$(AJGDIR)/bin
	@mkdir -p $(DESTDIR)$(AJGDIR)/www
	@mkdir -p $(DESTDIR)$(AJGDIR)/www/fakemod
	@install --strip $(BUILT)/ajg-daemon $(DESTDIR)$(AJGDIR)/bin
	@cp -r $(SAMPL)/* $(DESTDIR)$(AJGDIR)/www/fakemod

	@echo ========================================================
	@echo  AJG install OK [check with ajg-daemon --help]
	@echo ========================================================

install: build
	@(cd /usr/local/bin; ln -fs $(DESTDIR)$(AJGDIR)/bin/* .)

update: $(DIST)/http
	cp mixer-server $(DESTDIR)$(AJGDIR)/bin

