#pragma once

#include  "interrupt.h"
#include "type.h"
#include "vcpu.h"

#define IRQ_BASE 32
#define LAPIC_BASE 0xfee00000

struct lapic {
    u32 regs[1024];
    struct irr_queue *irr;
};

void emulate_lapicw(struct vcpu *vcpu);