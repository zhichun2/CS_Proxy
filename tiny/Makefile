CC = gcc
CFLAGS = -g -Og -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
	 -Wno-unused-parameter -Werror
CPPFLAGS = -I.. -D_FORTIFY_SOURCE=2 -D_XOPEN_SOURCE=700
# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LDLIBS = -Wl,--as-needed -lpthread

FILES = tiny tiny-static cgi-bin/adder

all: $(FILES)

tiny: tiny.c csapp.o
tiny-static: tiny-static.c csapp.o
cgi-bin/adder: cgi-bin/adder.c

csapp.o: ../csapp.c
	$(COMPILE.c) -o $@ $<

tar:
	(cd ..; tar cvf tiny.tar tiny)

clean:
	rm -f *.o *~ $(FILES)
