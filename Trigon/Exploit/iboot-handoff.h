// iboot-handoff.h
// Trigon, 2025

#ifndef IBOOT_HANDOFF_H
#define IBOOT_HANDOFF_H

#include <stdint.h>

#define IBOOT_HANDOFF_MAGIC (0x484F6666)

void parse_iboot_handoff(void *buffer, uint64_t *gPhysBase, uint64_t *gPhysSize, uint64_t *handoffBase, uint64_t *aopBase, uint64_t *aopSize);

#endif // IBOOT_HANDOFF_H
