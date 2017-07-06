IDIR =.
CC=gcc
CFLAGS=-I$(IDIR) -Wall -O2

ODIR=.
LDIR =.

LIBS=-lm -lpthread -lwiringPi

_DEPS = ethos.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = ethos.o spi_bitbang.o queue.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ethos: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ $(INCDIR)/*~ 
