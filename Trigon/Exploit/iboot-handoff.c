// iboot-handoff.c
// Trigon, 2025

#include "iboot-handoff.h"
#include "info.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
} iboot_handoff_header_t;

typedef struct {
    uint64_t regionID;
    uint64_t base;
    uint64_t size;
    uint64_t type;
} carveout_entry_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t nEntries;
    uint64_t unknown;
    carveout_entry_t entries[];
} handoff_memory_map_t;

#define HANDOFF_MEMORY_MAP_MAGIC {0x6D, 0x6D, 0x6F, 0x68}

#define IBOOT_HANDOFF_REGION_ID (1)
#define PRAM_REGION_ID (12)
#define VRAM_REGION_ID (14)
#define DRAM_REGION_ID (24)
#define AOP_REGION_ID  (31)

void *find_carveout_entry_for_id(void *entries, int nEntries, int id) {
    size_t entrySize = sizeof(carveout_entry_t);
    if (gDeviceInfo.major < 14) entrySize -= 8;

    for (int i = 0; i < nEntries; i++) {
        void *entry = (uint8_t *)entries + (i * entrySize);
        uint64_t entryRegionID = ((carveout_entry_t *)entry)->regionID;

        if (entryRegionID == id) {
            return entry;
        }
    }
    return NULL;
}

void parse_iboot_handoff(void *buffer, uint64_t *gPhysBase, uint64_t *gPhysSize, uint64_t *handoffBase, uint64_t *aopBase, uint64_t *aopSize) {
    iboot_handoff_header_t *header = (iboot_handoff_header_t *)buffer;

    handoff_memory_map_t *memoryMap = (handoff_memory_map_t *)memmem(header, header->size, (uint8_t [])HANDOFF_MEMORY_MAP_MAGIC, 4 * sizeof(uint8_t));
    if (!memoryMap) {
        printf("ERROR: failed to find handoff memory map\n");
        return;
    }

    carveout_entry_t *ibootHandoffEntry = find_carveout_entry_for_id(memoryMap->entries, memoryMap->nEntries, IBOOT_HANDOFF_REGION_ID);
    carveout_entry_t *dramEntry = find_carveout_entry_for_id(memoryMap->entries, memoryMap->nEntries, DRAM_REGION_ID);
    if (aopBase || aopSize) {
        carveout_entry_t *aopEntry = find_carveout_entry_for_id(memoryMap->entries, memoryMap->nEntries, AOP_REGION_ID);
        if (!aopEntry) goto out;
        if (aopBase) *aopBase = aopEntry->base;
        if (aopSize) *aopSize = aopEntry->size;
    }
out:
    if (gPhysBase) *gPhysBase = dramEntry->base;
    if (gPhysSize) *gPhysSize = dramEntry->size;
    if (handoffBase) *handoffBase = ibootHandoffEntry->base;
}
