COMMON_HEADERS = include/omrx.h
STATIC_LIBS = lib/libomrx.a
APPS = bin/test_read bin/test_write

CFLAGS = -Wall -Iinclude

all: $(APPS)

%.o: %.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

lib/libomrx.a: libomrx/libomrx.o
	mkdir -p lib
	ar rcs $@ $^

bin/%: apps/%.o $(STATIC_LIBS)
	mkdir -p bin
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -f libomrx/*.o apps/*.o

distclean: clean
	rm -f $(STATIC_LIBS) $(APPS)

