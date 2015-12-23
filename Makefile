LIBTYPE=static

COMMON_HEADERS = include/omrx.h
STATIC_LIB = lib/libomrx.a
SHARED_LIB = lib/libomrx.so
APPS = bin/test_read bin/test_write

CFLAGS = -Wall -Iinclude
LIB_CFLAGS = -fPIC
LDFLAGS = -Llib
LIBS = -lomrx

ifeq ($(LIBTYPE),dynamic)
  LD_LIBFLAGS =
  LD_ENDFLAGS =
  LIBFILE = $(SHARED_LIB)
else
  LD_LIBFLAGS = -Wl,-Bstatic
  LD_ENDFLAGS = -Wl,-Bdynamic
  LIBFILE = $(STATIC_LIB)
endif

all: $(LIBFILE) $(APPS) docs

docs: api-docs internal-docs

api-docs: doc/api/.buildstamp

internal-docs: doc/internal/.buildstamp

doc/api/.buildstamp: include/*.h src/*.c
	@mkdir -p doc
	doxygen Doxyfile.api >/dev/null
	@touch $@

doc/internal/.buildstamp: include/*.h src/*.c
	@mkdir -p doc
	doxygen Doxyfile.internal >/dev/null
	@touch $@

.SECONDARY:

src/%.o: src/%.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) $(LIB_CFLAGS) -c $< -o $@

apps/%.o: apps/%.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(STATIC_LIB): src/libomrx.o
	@mkdir -p lib
	$(AR) rcs $@ $^

$(SHARED_LIB): src/libomrx.o
	@mkdir -p lib
	$(CC) -shared $< -o $@

bin/%: apps/%.o $(LIBFILE)
	@mkdir -p bin
	$(CC) $(LDFLAGS) $< $(LD_LIBFLAGS) $(LIBS) $(LD_ENDFLAGS) -o $@

clean:
	rm -f src/*.o apps/*.o

distclean: clean
	rm -rf bin lib doc

