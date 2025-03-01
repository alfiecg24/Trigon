// pv.h
// Trigon, 2025

#ifndef PV_H
#define PV_H

#include <stdint.h>

/* All flags listed below are stored in the PV head pointer unless otherwise noted */
#define PVH_FLAG_IOMMU       0x4UL /* Stored in each PTE, or in PV head for single-PTE PV heads */
#define PVH_FLAG_IOMMU_TABLE (1ULL << 63) /* Stored in each PTE, or in PV head for single-PTE PV heads */
#define PVH_FLAG_CPU         (1ULL << 62)
#define PVH_LOCK_BIT         61
#define PVH_FLAG_LOCK        (1ULL << PVH_LOCK_BIT)
#define PVH_FLAG_EXEC        (1ULL << 60)
#define PVH_FLAG_LOCKDOWN    (1ULL << 59)
#define PVH_FLAG_HASHED      (1ULL << 58) /* Used to mark that a page has been hashed into the hibernation image. */
#define PVH_HIGH_FLAGS       (PVH_FLAG_CPU | PVH_FLAG_LOCK | PVH_FLAG_EXEC | PVH_FLAG_LOCKDOWN | PVH_FLAG_HASHED)
#define PVH_LIST_MASK   (~PVH_TYPE_MASK)

/* pvhead type */
#define PVH_TYPE_NULL        0x0UL
#define PVH_TYPE_PVEP        0x1UL
#define PVH_TYPE_PTEP        0x2UL
#define PVH_TYPE_PTDP        0x3UL

#define PVH_TYPE_MASK        (0x3UL)

#define pvh_get_flags(h)    (h & PVH_HIGH_FLAGS)

#define pvh_test_type(h, b)  ((h & PVH_TYPE_MASK) == (b))

#define pvh_ptep(h)   ((h & PVH_LIST_MASK) | PVH_HIGH_FLAGS)
#define pvh_list(h)    ((h & PVH_LIST_MASK) | PVH_HIGH_FLAGS)

#define atop(x) ((vm_address_t)(x) >> vm_kernel_page_shift)
#define ptoa(x) ((vm_address_t)(x) << vm_kernel_page_shift)
#define pa_index(pa) (atop(pa - gDeviceInfo.gPhysBase))

uint64_t pai_get_pvh(uint64_t pai);

#endif // PV_H
