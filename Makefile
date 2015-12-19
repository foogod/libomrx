COMMON_HEADERS = include/omrx.h
LIBOMRX = lib/libomrx.a
APPS = bin/test_read bin/test_write

CFLAGS = -Wall -Iinclude
LDFLAGS = -Llib
LIBS = -lomrx

all: $(APPS)

.SECONDARY:

%.o: %.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBOMRX): libomrx/libomrx.o
	@mkdir -p lib
	$(AR) rcs $@ $^

bin/%: apps/%.o $(LIBOMRX)
	@mkdir -p bin
	$(CC) $(LDFLAGS) $< $(LIBS) -o $@

clean:
	rm -f libomrx/*.o apps/*.o

distclean: clean
	rm -f $(LIBOMRX) $(APPS)

