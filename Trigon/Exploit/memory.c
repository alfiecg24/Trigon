// memory.c
// Trigon, 2025

#include "memory.h"
#include "info.h"
#include "mach_vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

uint64_t map_data(uint64_t pa, size_t size, vm_prot_t prot) {
    uint64_t offset = pa - gMappingBase;
    mach_port_t task = mach_task_self();
    uint64_t addr = 0;
    
    if (mach_vm_map(task, &addr, size, 0, 1, gExploitEntry, offset, 0, prot, prot, 0) != 0) return 0;
    return addr;
}

uint64_t map_data_with_offset(uint64_t offset, size_t size, vm_prot_t prot) {
    mach_port_t task = mach_task_self();
    uint64_t addr = 0;

    if (mach_vm_map(task, &addr, size, 0, 1, gExploitEntry, offset, 0, prot, prot, 0) != 0) return 0;
    return addr;
}

uint64_t map_page(uint64_t pa, vm_prot_t prot) {
    return map_data(pa, pages(1), prot);
}

void unmap_data(uint64_t addr, size_t size) {
    if (addr == 0) return;
    mach_vm_deallocate(mach_task_self(), addr, size);
}

void unmap_page(uint64_t addr) {
    unmap_data(addr, pages(1));
}

void physreadbuf(uint64_t pa, void *data, size_t size) {
    uint64_t offset = pa - trunc_page_kernel(pa);
    uint64_t mapped = map_data(trunc_page_kernel(pa), size + offset, VM_PROT_READ);
    if (mapped == 0) return;
    
    memcpy(data, (void *)(mapped + offset), size);
    unmap_data(mapped, size + offset);
}

void physwritebuf(uint64_t pa, void *data, size_t size) {
    uint64_t offset = pa - trunc_page_kernel(pa);
    uint64_t mapped = map_data(trunc_page_kernel(pa), size + offset, VM_PROT_READ | VM_PROT_WRITE);
    if (mapped == 0) return;
    
    memcpy((void *)(mapped + offset), data, size);
    unmap_data(mapped, size + offset);
}

uint64_t physread64(uint64_t pa) {
    uint64_t data = 0;
    physreadbuf(pa, &data, sizeof(uint64_t));
    return data;
}

uint32_t physread32(uint64_t pa) {
    uint32_t data = 0;
    physreadbuf(pa, &data, sizeof(uint32_t));
    return data;
}

void physwrite64(uint64_t pa, uint64_t data) {
    physwritebuf(pa, &data, sizeof(uint64_t));
}

void physwrite32(uint64_t pa, uint32_t data) {
    physwritebuf(pa, &data, sizeof(uint32_t));
}
