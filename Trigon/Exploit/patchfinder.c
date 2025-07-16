// patchfinder.c
// Trigon, 2025
// Based on https://github.com/xerub/patchfinder64 by Xerub

#include "patchfinder.h"
#include "info.h"
#include "mach_vm.h"
#include "memory.h"

#include <mach/mach.h>
#include <mach-o/loader.h>

#define INSN_ADRP 0x90000000, 0x9F000000

static uint64_t text_base = 0;
static uint64_t text_base_pa = 0;
static uint64_t text_size = 0;
static uint64_t cstring_base = 0;
static uint64_t cstring_base_pa = 0;
static uint64_t cstring_size = 0;
static uint64_t data_base = 0;
static uint64_t data_base_pa = 0;
static uint64_t data_size = 0;
static uint64_t kernel_entry = 0;

uint8_t *bh_memmem(uint8_t *data, size_t data_size, uint8_t* target, size_t target_size) {
    size_t skip[256] = {0};
    size_t last = target_size - 1;
    size_t scan = 0;
    
    for (; scan <= 255; scan++) skip[scan] = target_size;
    for (scan = 0; scan < last; scan++) skip[target[scan]] = last - scan;

    while (data_size >= target_size) {
        for (scan = last; data[scan] == target[scan]; scan = scan - 1) {
            if (scan == 0) return (uint8_t *)data;
        }
           
        data_size -= skip[data[last]];
        data += skip[data[last]];
    }
    return NULL;
}

uint64_t xref64(uint8_t *buf, uint64_t start, uint64_t end, uint64_t what) {
    uint64_t i;
    uint64_t value[32];
    memset(value, 0, sizeof(value));
    end &= ~3;
    
    for (i = start & ~3; i < end; i += 4) {
        uint32_t op = *(uint32_t *)(buf + i);
        unsigned reg = op & 0x1F;
        if ((op & 0x9F000000) == 0x90000000) {
            signed adr = ((op & 0x60000000) >> 18) | ((op & 0xFFFFE0) << 8);
            value[reg] = ((long long)adr << 1) + (i & ~0xFFF);
            continue;
        } else if ((op & 0xFF000000) == 0x91000000) {
            unsigned rn = (op >> 5) & 0x1F;
            unsigned shift = (op >> 22) & 3;
            unsigned imm = (op >> 10) & 0xFFF;
            if (shift == 1) imm <<= 12;
            else if (shift > 1) continue;
            value[reg] = value[rn] + imm;
        } else if ((op & 0xF9C00000) == 0xF9400000) {
            unsigned rn = (op >> 5) & 0x1F;
            unsigned imm = ((op >> 10) & 0xFFF) << 3;
            if (!imm) continue;
            value[reg] = value[rn] + imm;
        } else if ((op & 0x9F000000) == 0x10000000) {
            signed adr = ((op & 0x60000000) >> 18) | ((op & 0xFFFFE0) << 8);
            value[reg] = ((long long)adr >> 11) + i;
        } else if ((op & 0xFF000000) == 0x58000000) {
            unsigned adr = (op & 0xFFFFE0) >> 3;
            value[reg] = adr + i;
        }
        if (value[reg] == what) return i;
    }
    return 0;
}

uint64_t find_str(const char *str) {
    size_t size = round_page_kernel(cstring_size);
    uint64_t mapped = map_data(cstring_base_pa, size, VM_PROT_READ);
    
    uint8_t *loc = bh_memmem((uint8_t *)mapped, size, (uint8_t *)str, strlen(str));
    mach_vm_deallocate(mach_task_self(), mapped, size);
    if (loc == NULL) return 0;
    
    uint64_t offset = (uint64_t)loc - mapped;
    return cstring_base_pa + offset;
}

uint64_t find_strref(const char *str) {
    uint64_t str_addr = find_str(str);
    if (str_addr == 0) return 0;

    uint64_t kva = (str_addr - gDeviceInfo.kernelPhysBase) + gDeviceInfo.kernelBase;
    size_t size = round_page_kernel(text_size);
    uint64_t mapped = map_data(text_base_pa, size, VM_PROT_READ);

    uint64_t ref = xref64((uint8_t *)mapped, 0, size, kva-text_base);
    mach_vm_deallocate(mach_task_self(), mapped, size);
    if (ref == 0) return 0;
    return text_base_pa + ref;
}

uint64_t bof64(const uint8_t *buf, uint64_t start, uint64_t where) {
    for (; where >= start; where -= 4) {
        uint32_t op = *(uint32_t *)(buf + where);
        if ((op & 0xFFC003FF) == 0x910003FD) {
            unsigned delta = (op >> 10) & 0xFFF;
            if ((delta & 0xF) == 0) {
                uint64_t prev = where - ((delta >> 4) + 1) * 4;
                uint32_t au = *(uint32_t *)(buf + prev);
                if ((au & 0xFFC003E0) == 0xA98003E0) return prev;
                
                while (where > start) {
                    where -= 4;
                    au = *(uint32_t *)(buf + where);
                    if ((au & 0xFFC003FF) == 0xD10003FF && ((au >> 10) & 0xFFF) == delta + 0x10) return where;
                    if ((au & 0xFFC003E0) != 0xA90003E0) {
                        where += 4;
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

uint64_t step64(const uint8_t *buf, uint64_t start, size_t length, uint32_t what, uint32_t mask) {
    uint64_t end = start + length;
    while (start < end) {
        uint32_t x = *(uint32_t *)(buf + start);
        if ((x & mask) == what) {
            return start;
        }
        start += 4;
    }
    return 0;
}

uint64_t calc64(const uint8_t *buf, uint64_t start, uint64_t end, int which) {
    uint64_t i;
    uint64_t value[32];
    memset(value, 0, sizeof(value));
    end &= ~3;

    for (i = start & ~3; i < end; i += 4) {
        uint32_t op = *(uint32_t *)(buf + i);
        unsigned reg = op & 0x1F;
        if ((op & 0x9F000000) == 0x90000000) {
            signed adr = ((op & 0x60000000) >> 18) | ((op & 0xFFFFE0) << 8);
            value[reg] = ((long long)adr << 1) + (i & ~0xFFF);
        } else if ((op & 0xFF000000) == 0x91000000) {
            unsigned rn = (op >> 5) & 0x1F;
            unsigned shift = (op >> 22) & 3;
            unsigned imm = (op >> 10) & 0xFFF;
            if (shift == 1) imm <<= 12;
            else if (shift > 1) continue;
            value[reg] = value[rn] + imm;
        } else if ((op & 0xF9C00000) == 0xF9400000) {
            unsigned rn = (op >> 5) & 0x1F;
            unsigned imm = ((op >> 10) & 0xFFF) << 3;
            if (!imm) continue;
            value[reg] = value[rn] + imm;
        } else if ((op & 0xF9C00000) == 0xF9000000) {
            unsigned rn = (op >> 5) & 0x1F;
            unsigned imm = ((op >> 10) & 0xFFF) << 3;
            if (!imm) continue;
            value[rn] = value[rn] + imm;
        } else if ((op & 0x9F000000) == 0x10000000) {
            signed adr = ((op & 0x60000000) >> 18) | ((op & 0xFFFFE0) << 8);
            value[reg] = ((long long)adr >> 11) + i;
        } else if ((op & 0xFF000000) == 0x58000000) {
            unsigned adr = (op & 0xFFFFE0) >> 3;
            value[reg] = adr + i;
        }
    }
    return value[which];
}

uint64_t follow_call64(const uint8_t *buf, uint64_t call) {
    long long w;
    w = *(uint32_t *)(buf + call) & 0x3FFFFFF;
    w <<= 64 - 26;
    w >>= 64 - 26 - 2;
    return call + w;
}\

int memcmp_masked(const void *str1, const void *str2, unsigned char* mask, size_t n)
{
    const unsigned char* p = (const unsigned char*)str1;
    const unsigned char* q = (const unsigned char*)str2;

    if (p == q) return 0;
    for (int i = 0; i < n; i++) {
        unsigned char cMask = 0xFF;
        if (mask) {
            cMask = mask[i];
        }
        if((p[i] & cMask) != (q[i] & cMask)) {
            // we do not care about 1 / -1
            return -1;
        }
    }

    return 0;
}

uint64_t find_prev_inst(uint64_t pa, int nInstrs, uint32_t instr, uint32_t mask) {
    uint64_t start = pa;
    uint64_t end = pa - (nInstrs * 4);
    unsigned char mask_bytes[4] = {
        mask & 0xFF,
        (mask >> 8) & 0xFF,
        (mask >> 16) & 0xFF,
        (mask >> 24) & 0xFF
    };

    for (; start >= end; start -= 4) {
        uint32_t curInstr = physread32(start);
        if (memcmp_masked(&curInstr, &instr, mask_bytes, 4) == 0) {
            return start;
        }
    }

    return 0;
}

#define ADRP_PAGE_SIZE 0x1000
#define ADRP_PAGE_MASK 0x0fff

int64_t sxt64(int64_t value, uint8_t bits)
{
    value = ((uint64_t)value) << (64 - bits);
    value >>= (64 - bits);
    return value;
}

int arm64_dec_adr_p(uint32_t inst, uint64_t origin, uint64_t *targetOut)
{
    bool isAdrp = false;
    if ((inst & 0x9f000000) == 0x90000000) {
        isAdrp = true;
    }
    else if ((inst & 0x9f000000) == 0x10000000) {
        isAdrp = false;
    }
    else {
        return -1;
    }

    if (targetOut) {
        uint64_t offset = ((inst >> 29) & 0x3) | ((inst >> 3) & 0x1ffffc);
        if (isAdrp) {
            offset = (offset << 12);
            origin &= ~ADRP_PAGE_MASK;
        }
        int64_t signedOffset = sxt64(offset, 33);
        *targetOut = origin + signedOffset;
    }

    return 0;
}

static int _arm64_dec_str_ldr_imm(uint32_t inst, uint64_t *immOut)
{
    bool isUnsigned = inst & (1 << 24);
    if (!isUnsigned) {
        bool isUnscaled = !(inst & (1 << 10));
        if (isUnscaled) return -1;
    }

    uint8_t size = (inst >> 30);
    bool isXReg = size == 0b11;

    if (immOut) {
        if (isUnsigned) {
            uint64_t imm = ((inst >> 10) & 0xfff) * (isXReg ? 8 : 4);
            *immOut = imm;
        }
        else {
            uint64_t imm = ((inst >> 12) & 0x1ff);
            *immOut = sxt64(imm, 9);
        }
    }

    return 0;
}

int arm64_dec_ldr_imm(uint32_t inst, uint64_t *immOut)
{
    if ((inst & 0x3a400000) != 0x38400000) return -1;
    return _arm64_dec_str_ldr_imm(inst, immOut);
}

uint64_t find_pv_head_table(void) {
    uint64_t ref = find_strref("\"%s: unexpected PV head %p, pte_p=%p pmap=%p pv_h=%p\"");
    if (ref == 0) ref = find_strref("%s: unexpected PV head %p, ptep=%p pmap=%p pvh=%p pai=0x%x @%s:%d");
    if (ref == 0) goto fallback;

    size_t size = round_page_kernel(text_size);
    uint64_t mapped = map_data(text_base_pa, size, VM_PROT_READ);
    uint64_t start = bof64((uint8_t *)mapped, 0, ref-text_base_pa);
    if (start == 0) {
        mach_vm_deallocate(mach_task_self(), mapped, size);
        goto fallback;
    }
    
    uint64_t adrp = step64((uint8_t *)mapped, start, 0x180, INSN_ADRP);
    if (adrp == 0) {
        mach_vm_deallocate(mach_task_self(), mapped, size);
        goto fallback;
    }
    
    uint64_t target = 0;
    arm64_dec_adr_p(*(uint32_t *)(mapped + adrp), text_base + adrp, &target);
    uint64_t immed = 0;
    arm64_dec_ldr_imm(*(uint32_t *)(mapped + adrp + 4), &immed);
    unmap_data(mapped, size);
    
    uint64_t addr = target + immed;
    if (addr != 0) return addr;

fallback:
    ref = find_strref("pmap_iommu_unmap_internal");
    if (ref == 0) return 0;
    
    size = round_page_kernel(text_size);
    mapped = map_data(text_base_pa, size, VM_PROT_READ);
    start = bof64((uint8_t *)mapped, 0, ref-text_base_pa);
    if (start == 0) {
        mach_vm_deallocate(mach_task_self(), mapped, size);
        return 0;
    }
    
    adrp = step64((uint8_t *)mapped, start, 0x120, INSN_ADRP);
    if (adrp == 0) {
        mach_vm_deallocate(mach_task_self(), mapped, size);
        return 0;
    }
    
    uint64_t calc = calc64((uint8_t *)mapped, adrp, adrp+8, 24);
    if (calc == 0) calc = calc64((uint8_t *)mapped, adrp, adrp+8, 8);
    if (calc == 0) {
        mach_vm_deallocate(mach_task_self(), mapped, size);
        return 0;
    }
    
    mach_vm_deallocate(mach_task_self(), mapped, size);
    return physread64(calc + text_base_pa);
}

uint64_t find_cpu_ttep(void) {
    uint64_t entry_pa = (kernel_entry + gDeviceInfo.kernelSlide) - gDeviceInfo.kernelBase + gDeviceInfo.kernelPhysBase;
    uint32_t branch = physread32(entry_pa);
    uint64_t loc = branch & 0x3FFFFFF;
    
    loc <<= 64 - 26;
    loc >>= 64 - 26 - 2;
    uint64_t addr = loc + entry_pa;
    addr -= text_base_pa;

    size_t size = round_page_kernel(text_size);
    uint64_t mapped = map_data(text_base_pa, size, VM_PROT_READ);
    uint64_t mov = step64((uint8_t *)mapped, addr, 0x4000, 0xd2c08001, 0xffffffff);
    if (mov == 0) {
        mach_vm_deallocate(mach_task_self(), mapped, size);
        return 0;
    }

    uint64_t adrp = step64((uint8_t *)mapped, mov, 0x40, INSN_ADRP);
    if (adrp == 0) {
        mach_vm_deallocate(mach_task_self(), mapped, size);
        return 0;
    }
    
    uint64_t calc = calc64((uint8_t *)mapped, adrp, adrp+8, 0);
    mach_vm_deallocate(mach_task_self(), mapped, size);
    if (!calc) return 0;
    return physread64(calc + text_base_pa);
}

int find_kernel_offsets(void) {
    uint64_t hdr_page = map_data(gDeviceInfo.kernelPhysBase, 0x10000, VM_PROT_READ);
    struct mach_header_64 *hdr = (struct mach_header_64 *)hdr_page;
    struct load_command *load_cmd = (struct load_command *)(hdr + 1);
    
    for (int i = 0; i < hdr->ncmds; i++) {
        if (load_cmd->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *segment = (struct segment_command_64 *)load_cmd;
            struct section_64 *section = (struct section_64 *)(segment + 1);
            
            if (strcmp(segment->segname, "__TEXT_EXEC") == 0) {
                for (int j = 0; j < segment->nsects; j++) {
                    if (strcmp(section[j].sectname, "__text") == 0) {
                        text_base = section[j].addr;
                        text_size = section[j].size;
                    }
                }
            } else if (strcmp(segment->segname, "__TEXT") == 0) {
                for (int j = 0; j < segment->nsects; j++) {
                    if (strcmp(section[j].sectname, "__cstring") == 0) {
                        cstring_base = section[j].addr;
                        cstring_size = section[j].size;
                        if (text_base != 0) break;
                    } else if (strcmp(section[j].sectname, "__text") == 0) {
                        if (text_base == 0) {
                            text_base = section[j].addr;
                            text_size = section[j].size;
                        }
                        if (cstring_base != 0) break;
                    }
                }
            } else if (strcmp(segment->segname, "__PRELINK_INFO") == 0) {
                for (int j = 0; j < segment->nsects; j++) {
                    if (strcmp(section[j].sectname, "__info") == 0) {
                        gDeviceInfo.kernelEnd = section[j].addr;
                        break;
                    }
                }
            }
            
            if (strcmp(segment->segname, "__DATA") == 0) {
               for (int j = 0; j < segment->nsects; j++) {
                   if (strcmp(section[j].sectname, "__data") == 0) {
                       data_base = section[j].addr;
                       data_size = section[j].size;
                       break;
                   }
               }
           }
        } else if (load_cmd->cmd == LC_UNIXTHREAD) {
            uint32_t *ptr = (uint32_t *)(load_cmd + 1);
            uint32_t flavor = ptr[0];
            struct {
                uint64_t x[29];
                uint64_t fp;
                uint64_t lr;
                uint64_t sp;
                uint64_t pc;
                uint32_t cpsr;
            } *thread = (void *)(ptr + 2);
            if (flavor == 6) {
                kernel_entry = thread->pc;
            }
        }
    
        if (kernel_entry && text_base && cstring_base && data_base) break;
        load_cmd = (struct load_command *)((uint64_t)load_cmd + load_cmd->cmdsize);
    }
    
    text_base_pa = trunc_page_kernel((text_base - gDeviceInfo.kernelBase) + gDeviceInfo.kernelPhysBase);
    cstring_base_pa = trunc_page_kernel((cstring_base - gDeviceInfo.kernelBase) + gDeviceInfo.kernelPhysBase);
    data_base_pa = trunc_page_kernel((data_base - gDeviceInfo.kernelBase) + gDeviceInfo.kernelPhysBase);
    gDeviceInfo.pv_head_table = find_pv_head_table();
    printf("pv_head_table: 0x%llX\n", gDeviceInfo.pv_head_table);
    gDeviceInfo.cpu_ttep = find_cpu_ttep();
    printf("CPU TTEP: 0x%llX\n", gDeviceInfo.cpu_ttep);
    
    if (!gDeviceInfo.pv_head_table || !gDeviceInfo.cpu_ttep) return -1;

    return 0;
}
