#ifndef BASE58_H
#define BASE58_H

#include <stdint.h>
#include <stddef.h>

int base58check_encode(const uint8_t *hash160, size_t hash160_len, uint8_t version, char *output, size_t output_size);

#endif
