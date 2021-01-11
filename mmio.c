#include "mmio.h"

void emulate_mmio(struct vcpu *vcpu) {
    struct mmio mmio = {
        .data = vcpu->kvm_run->mmio.data,
        .is_write = vcpu->kvm_run->mmio.is_write,
        .len = vcpu->kvm_run->mmio.len,
        .phys_addr = vcpu->kvm_run->mmio.phys_addr
    };

    if (mmio.phys_addr >= LAPIC_BASE)
        emulate_lapicw(vcpu);
    if (mmio.phys_addr >= IOAPIC_BASE) {
        emulate_ioapicw(vcpu);
    }
}