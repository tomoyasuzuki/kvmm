#pragma once

#include  "interrupt.h"
#include "type.h"

#define IRQ_BASE 32

struct lapic {
    u32 regs[1024];
    struct irr_queue *irr;
};