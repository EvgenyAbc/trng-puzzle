# trng-puzzle

Bitcoin vanity address search using Infinite Noise Multiplier TRNG hardware.

## Algorithm

1. **Random seed generation** — Raw entropy is read from an [InfNoise](https://github.com/leetronics/infnoise) True Random Number Generator (TRNG) over FTDI in batches of 5120 bytes (160 seeds of 32 bytes each).

2. **Seed rotation** — Each 32-byte seed is rotated bitwise 256 times (`ror_bits`), generating 256 distinct candidate private keys per seed (153,600 keys per TRNG batch). The rotation shifts all bytes by `n` bits with cross-byte carry.

3. **Key constraining** — Each candidate key is masked to `bitsNum` bits: the top (`bitsNum`-1) bits are zeroed, and bit (`bitsNum`-1) is forced to 1, ensuring the key lies in `[2^(bitsNum-1), 2^bitsNum-1)`.

4. **Address derivation**:
   - EC point multiplication: `pub = key * G` on secp256k1 using a double-and-add scalar multiplication and modular arithmetic over the prime field.
   - Compressed public key: `0x02`/`0x03` (y parity) + x coordinate (33 bytes).
   - `SHA256(compressed_pubkey)` → `RIPEMD160(hash)` → `Base58Check(0x00, hash160)`.

5. **Comparison** — Derived address is compared to the target from `addresses.json`. On match, the private key is written to `found/<address>`.

6. **Parallelism** — Multiple worker threads share the single TRNG device via a mutex. Each thread processes a batch independently, checking `*found` between rotations for early exit.

## Usage

```
// stop deamon
sudo systemctl stop infnoise

// bcz direct use
sudo ./trng-puzzle [threads]
sudo ./trng-puzzle -p [threads]    # print every key in binary to stdout
```

## Build

```
make
```

## Dependencies

- InfNoise TRNG hardware + `libinfnoise`
- `libftdi1`
- OpenSSL (libcrypto) for SHA256 and RIPEMD160
