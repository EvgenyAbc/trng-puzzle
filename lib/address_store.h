#ifndef ADDRESS_STORE_H
#define ADDRESS_STORE_H

typedef struct {
    int bitsNum;
    char address[100];
} AddressEntry;

int loadAddressEntries(const char *jsonPath, AddressEntry **outEntries, int *outCount);
int advanceEntryIndex(int count, int *currentIndex);
void freeAddressEntries(AddressEntry *entries, int count);

#endif
