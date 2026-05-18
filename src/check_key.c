#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "crypto.h"
#include "secp256k1.h"
#include "base58.h"

int main() {
    uint8_t key[32] = {0};
    key[30] = 0xC9;
    key[31] = 0x36;

    printf("Private key: ");
    for (int i = 0; i < 32; i++) printf("%02x", key[i]);
    printf("\n\n");

    /* EC multiplication */
    uint256 k;
    uint256_from_be_bytes(&k, key);
    ecpoint G = secp256k1_G();
    ecpoint pub = secp256k1_multiply(&k, &G);

    /* === Correct compressed pubkey: 0x02/0x03 + x (32 bytes) === */
    uint8_t cpk[33];
    cpk[0] = (pub.y.v[0] & 1) ? 0x03 : 0x02;  /* parity from y LSB */
    uint256_to_be_bytes(&pub.x, cpk + 1);      /* x coordinate */

    printf("Compressed pubkey: ");
    for (int i = 0; i < 33; i++) printf("%02x", cpk[i]);
    printf("\n");
    printf("Expected:          029d8c5d35231d75eb87fd2c5f05f65281ed9573dc41853288c62ee94eb2590b7a\n\n");

    /* SHA256 */
    uint8_t h1[32];
    SHA256(cpk, 33, h1);
    printf("SHA256(compressed): ");
    for (int i = 0; i < 32; i++) printf("%02x", h1[i]);
    printf("\n");

    /* RIPEMD160 */
    uint8_t h160[20];
    RIPEMD160(h1, 32, h160);
    printf("HASH160:            ");
    for (int i = 0; i < 20; i++) printf("%02x", h160[i]);
    printf("\n");

    /* Base58Check */
    char address[64];
    base58check_encode(h160, 20, 0x00, address, sizeof(address));
    printf("Address:            %s\n", address);
    printf("Target:             1BDyrQ6WoF8VN3g9SAS1iKZcPzFfnDVieY\n\n");

    if (strcmp(address, "1BDyrQ6WoF8VN3g9SAS1iKZcPzFfnDVieY") == 0) {
        printf("*** MATCH ***\n");
        return 0;
    } else {
        printf("MISMATCH\n");
        return 1;
    }
}
