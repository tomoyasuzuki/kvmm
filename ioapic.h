#pragma once

#include "type.h"
#include "vcpu.h"

union redirtb_entry {
    struct {
        u64 vec : 8;
        u64 deliverly_mode : 3;
        u64 dest_mode : 1;
        u64 deliverly_status : 1;
        u64 pin_polarity : 1;
        u64 remote_irr : 1;
        u64 trigger_mode : 1;
        u64 mask : 1;
        u64 : 39;
        u64 destination : 8;
    } fields;
    struct {
        u32 lower;
        u32 upper;
    } regs;
};

struct ioapic {
    u32 ioregsel;
    u32 iowin;
    u32 id;
    u32 vec;
    union redirtb_entry redirtb[24];
};

#define IOAPIC_BASE 0xfec00000
#define IOAPIC_REDRTB_BASE (IOAPIC_BASE + 0x10)

void init_ioapic();
void emulate_ioapicw(struct vcpu *vcpu);