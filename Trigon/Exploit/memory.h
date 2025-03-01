// memory.h
// Trigon, 2025

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <mach/mach.h>
#include <mach/mach_host.h>

uint64_t map_data(uint64_t pa, size_t size, vm_prot_t prot);
uint64_t map_data_with_offset(uint64_t offset, size_t size, vm_prot_t prot);
uint64_t map_page(uint64_t pa, vm_prot_t prot);
void unmap_data(uint64_t addr, size_t size);
void unmap_page(uint64_t addr);

void physreadbuf(uint64_t pa, void *data, size_t size);
void physwritebuf(uint64_t pa, void *data, size_t size);
uint64_t physread64(uint64_t pa);
uint32_t physread32(uint64_t pa);
void physwrite64(uint64_t pa, uint64_t data);
void physwrite32(uint64_t pa, uint32_t data);

#endif // MEMORY_H
