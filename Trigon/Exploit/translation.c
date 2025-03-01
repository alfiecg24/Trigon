// translation.c
// Trigon, 2025

#include "translation.h"

#include "memory.h"
#include "info.h"

#define ARM_TTE_PA_MASK 0x0000FFFFFFFFF000ull
#define PTE_TO_PA(pte) ((pte) & ARM_TTE_PA_MASK)

uint64_t vtophys(uint64_t va, uint64_t ttep) {
    uint64_t l1Index = (va >> 36) & 7;
    uint64_t l1Entry = physread64(ttep + (l1Index * sizeof(uint64_t)));
    if ((l1Entry & 3) != 3) return 0;

    uint64_t l2Table = l1Entry & 0xFFFFFFFFC000;
    uint64_t l2Index = (va >> 25) & 0x7FF;
    uint64_t l2Entry = physread64(l2Table + (l2Index * sizeof(uint64_t)));
    if ((l2Entry & 3) == 1) return PTE_TO_PA(l2Entry) | (va & 0x1FFFFFF);

    if ((l2Entry & 3) == 3) {
        uint64_t l3Table = l2Entry & 0xFFFFFFFFC000;
        uint64_t l3Index = (va >> 14) & 0x7FF;
        uint64_t l3Entry = physread64(l3Table + (l3Index * sizeof(uint64_t)));
        // Should always be a block mapping
        if ((l3Entry & 3) != 3) return 0;
        return PTE_TO_PA(l3Entry) | (va & 0x3FFF);
    }

    return 0;
}

uint64_t kvtophys(uint64_t va) {
    return vtophys(va, gDeviceInfo.cpu_ttep);
}
