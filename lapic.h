#pragma once

#include <sys/ioctl.h>
#include <pthread.h>
#include "type.h"
#include "vcpu.h"
#include "util.h"
#include "uart.h"

#define IRQ_BASE 32
#define LAPIC_BASE 0xfee00000

struct lapic {
    u32 regs[1024];
    int lock;
    struct irr_queue *irr;
};

struct irr_queue {
    int buff[10];
    int last;
};

void init_lapic();
void emulate_interrupt(struct vcpu *vcpu);
void enq_irr(struct vcpu *vcpu, int value);
int deq_irr();
void inject_interrupt(int vcpufd, int irq);
void emulate_lapicw(struct vcpu *vcpu);