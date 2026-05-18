#ifndef SECP256K1_H
#define SECP256K1_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t v[8];
} uint256;

typedef struct {
    uint256 x;
    uint256 y;
} ecpoint;

void uint256_from_be_bytes(uint256 *k, const uint8_t bytes[32]);
void uint256_to_be_bytes(const uint256 *k, uint8_t bytes[32]);

ecpoint secp256k1_G(void);
ecpoint secp256k1_multiply(const uint256 *k, const ecpoint *p);

#endif
