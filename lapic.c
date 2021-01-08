#include "lapic.h"

#define MAX_IRR_COUNT 10

struct lapic *lapic;


void init_lapic() {
    lapic = malloc(sizeof(struct lapic));
    lapic->irr = malloc(4096);
    lapic->lock = 0;
}

int irq_is_valid(int v) {
    return v >= 32;
}

void emulate_interrupt(struct vcpu *vcpu) {
    int irq;

    for (int i = 0; i < lapic->irr->last; i++) {
        irq = lapic->irr->buff[i];
        if (!irq_is_valid(irq)) {
            deq_irr();
            continue;
        }

        switch (irq) {
        case IRQ_BASE+4:
            inject_interrupt(vcpu->fd, irq);
            break;
        case IRQ_BASE+14:
            inject_interrupt(vcpu->fd, irq);
            break;
        default:
            continue;
            break;
        }

        //printf("inject: %d, count: %d\n", irq, lapic->irr->last);
    }

    vcpu->kvm_run->request_interrupt_window = 0;
}

void enq_irr(struct vcpu *vcpu, int value) {
    if (!lapic->lock) {
        lapic->irr->buff[lapic->irr->last] = value;
        lapic->irr->last++;
    }

    if (lapic->irr->last >= MAX_IRR_COUNT-1) {
        lapic->lock = 1;
    }

    vcpu->kvm_run->request_interrupt_window = 1;
}

int deq_irr() {
   int out = lapic->irr->buff[0];
   for (int i = 0; i < lapic->irr->last; i++) {
       lapic->irr->buff[i] = lapic->irr->buff[i+1];
   }

   lapic->irr->last--;
   
   if (lapic->irr->last < MAX_IRR_COUNT-1) {
       lapic->lock = 0;
   }
}  

void inject_interrupt(int vcpufd, int irq) {
    struct kvm_interrupt *intr = malloc(sizeof(struct kvm_interrupt));
    intr->irq = irq;
    
    if (ioctl(vcpufd, KVM_INTERRUPT, intr) < 0)
        error("KVM_INTERRUPT");
    
    deq_irr();
}

void emulate_lapicw(struct vcpu *vcpu) {
    int index = vcpu->kvm_run->mmio.phys_addr - LAPIC_BASE;
    u32 data = 0;

    for (int i = 0; i < 4; i++) {
        data |= vcpu->kvm_run->mmio.data[i] << i*8;
    }

    if (vcpu->kvm_run->mmio.is_write)
        lapic->regs[index/4] = data;
}