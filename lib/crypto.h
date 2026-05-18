#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

unsigned char *SHA256(const unsigned char *d, size_t n, unsigned char *md);
unsigned char *RIPEMD160(const unsigned char *d, size_t n, unsigned char *md);

#endif
