#pragma once

#include <sys/ioctl.h>
#include "type.h"
#include "vcpu.h"
#include "util.h"

#define IRQ_BASE 32
#define LAPIC_BASE 0xfee00000

struct lapic {
    u32 regs[1024];
    struct irr_queue *irr;
};

struct irr_queue {
    int buff[1000];
    int last;
};

void init_lapic();
void emulate_interrupt(struct vcpu *vcpu);
void enq_irr(struct irr_queue *irr, int value);
int deq_irr(struct irr_queue *irr);
void inject_interrupt(int vcpufd, int irq);
void emulate_lapicw(struct vcpu *vcpu);