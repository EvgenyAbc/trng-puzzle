#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <ftdi.h>
#include <libinfnoise.h>

int main(int argc, char *argv[])
{
    char *serial = NULL;
    bool keccak = true;
    uint32_t multiplier = 1;
    bool debug = false;
    uint64_t total = 1000000;

    if (argc > 1) total = strtoull(argv[1], NULL, 10);

    struct infnoise_context ctx;
    if (!initInfnoise(&ctx, serial, keccak, debug)) {
        fprintf(stderr, "initInfnoise failed: %s\n", ctx.message);
        return 1;
    }

    uint8_t buf[64];
    uint64_t written = 0;

    while (written < total) {
        ctx.errorFlag = false;
        uint32_t n = readData(&ctx, buf, !keccak, multiplier);
        if (ctx.errorFlag) {
            fprintf(stderr, "readData error: %s\n", ctx.message);
            return 1;
        }
        fwrite(buf, 1, n, stdout);
        written += n;
    }

    deinitInfnoise(&ctx);
    return 0;
}
