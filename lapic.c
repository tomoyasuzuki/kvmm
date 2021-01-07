#include "lapic.h"

void emulate_lapicw(struct vcpu *vcpu, struct lapic *lapic) {
    int index = vcpu->kvm_run->mmio.phys_addr - LAPIC_BASE;
    u32 data = 0;

    for (int i = 0; i < 4; i++) {
        data |= vcpu->kvm_run->mmio.data[i] << i*8;
    }

    if (vcpu->kvm_run->mmio.is_write)
        lapic->regs[index/4] = data;
}