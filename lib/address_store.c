#include "address_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int loadAddressEntries(const char *jsonPath, AddressEntry **outEntries, int *outCount)
{
    FILE *f = fopen(jsonPath, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", jsonPath);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        fprintf(stderr, "Empty or invalid file: %s\n", jsonPath);
        fclose(f);
        return -1;
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fprintf(stderr, "malloc failed\n");
        fclose(f);
        return -1;
    }

    size_t nread = fread(buf, 1, (size_t)len, f);
    fclose(f);

    if ((long)nread != len) {
        fprintf(stderr, "Read error\n");
        free(buf);
        return -1;
    }
    buf[len] = '\0';

    int capacity = 16;
    int count = 0;
    AddressEntry *entries = (AddressEntry *)malloc(sizeof(AddressEntry) * (size_t)capacity);

    const char *p = buf;
    while (*p) {
        p = strstr(p, "\"bitsNum\"");
        if (!p) break;
        p = strchr(p, ':');
        if (!p) break;
        p++;
        while (*p == ' ' || *p == '\t') p++;
        int bitsNum = atoi(p);

        p = strstr(p, "\"address\"");
        if (!p) break;
        p = strchr(p, ':');
        if (!p) break;
        p++;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '"') break;
        p++;
        int addrLen = 0;
        while (p[addrLen] && p[addrLen] != '"') addrLen++;
        if (addrLen <= 0 || addrLen >= 100) break;

        if (count >= capacity) {
            capacity *= 2;
            entries = (AddressEntry *)realloc(entries, sizeof(AddressEntry) * (size_t)capacity);
        }

        entries[count].bitsNum = bitsNum;
        memcpy(entries[count].address, p, (size_t)addrLen);
        entries[count].address[addrLen] = '\0';
        count++;

        p += addrLen;
    }

    free(buf);

    if (count == 0) {
        fprintf(stderr, "No valid entries found in %s\n", jsonPath);
        free(entries);
        return -1;
    }

    *outEntries = entries;
    *outCount = count;
    return 0;
}

int advanceEntryIndex(int count, int *currentIndex)
{
    *currentIndex = (*currentIndex + 1) % count;
    return *currentIndex;
}

void freeAddressEntries(AddressEntry *entries, int count)
{
    (void)count;
    free(entries);
}
