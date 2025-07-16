// info.h
// Trigon, 2025

#ifndef INFO_H
#define INFO_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <mach/mach.h>

extern uint64_t gMappingBase;
extern mach_port_t gExploitEntry;

typedef struct s_DeviceInfo {
    uint64_t pageSize;
    uint64_t iorvbar;
    bool aop;
    int major, minor, patch;
    
    uint64_t gPhysBase, gPhysSize;
    uint64_t kernelPhysBase, kernelBase, kernelSlide, kernelEnd;
    uint64_t pv_head_table, cpu_ttep;
} DeviceInfo;
extern DeviceInfo gDeviceInfo;

#define pages(x) (gDeviceInfo.pageSize * x)

void info_init(bool *supported);

#endif // INFO_H
