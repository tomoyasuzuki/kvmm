#include "lapic.h"

struct lapic *lapic;

void init_lapic() {
    lapic = malloc(sizeof(struct lapic));
    lapic->irr = malloc(4096);
}

void emulate_interrupt(struct vcpu *vcpu) {
    if (lapic->irr->buff[0] >= 32) {
        inject_interrupt(vcpu->fd, lapic->irr->buff[0]);
        deq_irr(lapic->irr);
        vcpu->kvm_run->request_interrupt_window = 0;
    }
}

void enq_irr(struct irr_queue *irr, int value) {
    irr->buff[irr->last] = value;
    irr->last++;
}

int deq_irr(struct irr_queue *irr) {
   int out = irr->buff[0];
   for (int i = 0; i <= irr->last; i++) {
       irr->buff[i] = irr->buff[i+1];
   }
   irr->last--;
}  

void inject_interrupt(int vcpufd, int irq) {
    struct kvm_interrupt *intr = malloc(sizeof(struct kvm_interrupt));
    intr->irq = irq;
    
    
    if (ioctl(vcpufd, KVM_INTERRUPT, intr) < 0)
        error("KVM_INTERRUPT");
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