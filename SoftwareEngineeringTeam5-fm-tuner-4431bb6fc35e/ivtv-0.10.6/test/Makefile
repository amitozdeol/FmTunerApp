INSTALLDIR = /usr/local/bin
HEADERDIR = /usr/include/linux

EXES = vbi wss vbi-passthrough vbi-detect ivtv-pcm-tester mpeg-read-test mpeg-freq-test

CFLAGS = -I../driver -I../utils -D_GNU_SOURCE -O2 -Wall
CXXFLAGS = $(CFLAGS)
LDFLAGS = -lm

all: $(EXES)

install: all

clean: 
	rm -f *.o $(EXES)
