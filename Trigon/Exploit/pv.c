// pv.c
// Trigon, 2025

#include "pv.h"
#include "info.h"
#include "memory.h"
#include "translation.h"

static uint64_t tablePA = 0;

uint64_t pai_get_pvh(uint64_t pai) {
    if (!tablePA) tablePA = kvtophys(physread64(kvtophys(gDeviceInfo.pv_head_table)));
    return physread64(tablePA + (pai * 8));
}
