CFLAGS = -Wall -Wextra -std=c99 -O2 -Ilib -I/usr/include/libftdi1 -pthread
LDLIBS = -linfnoise -lftdi1 -lm -lrt -lpthread
CRYPTOLIB = -l:libcrypto.so.3

SRC = src
LIB = lib

TRNG_OBJS = $(SRC)/main.o $(LIB)/secp256k1.o $(LIB)/base58.o $(LIB)/address_store.o

.PHONY: all clean

all: trng-read trng-puzzle check-key

trng-read: $(SRC)/trng-read.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

trng-puzzle: $(TRNG_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) $(CRYPTOLIB)

$(SRC)/main.o: $(SRC)/main.c lib/crypto.h lib/secp256k1.h lib/base58.h lib/address_store.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB)/secp256k1.o: $(LIB)/secp256k1.c lib/secp256k1.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB)/base58.o: $(LIB)/base58.c lib/base58.h lib/crypto.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB)/address_store.o: $(LIB)/address_store.c lib/address_store.h
	$(CC) $(CFLAGS) -c -o $@ $<

check-key: $(SRC)/check_key.c lib/secp256k1.c lib/base58.c
	$(CC) $(CFLAGS) -o $@ $^ $(CRYPTOLIB)

clean:
	rm -f trng-read trng-puzzle check-key $(SRC)/*.o $(LIB)/*.o
