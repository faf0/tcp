all: tcp tcpm

tcp: tcp.c
	cc -Wall -pedantic -DMMAP_FILES=0 tcp.c -o tcp -lbsd

tcpm: tcp.c
	cc -Wall -pedantic -DMMAP_FILES=1 tcp.c -o tcpm -lbsd

clean:
	rm -f tcp tcpm *~
