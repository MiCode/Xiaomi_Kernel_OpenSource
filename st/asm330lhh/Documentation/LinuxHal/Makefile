
IDIR = -I. -I../../linux/

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)gcc
CFLAGS=$(IDIR) -DLOG_TAG=\"test_linux\"
LIB=-ldl -lpthread -lm -lrt

.PHONY: test_linux

all: test_linux

OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

test_linux: test_linux.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIB)

clean:
	rm -f *.o 
