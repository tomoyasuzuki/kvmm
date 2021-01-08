#include "ioapic.h"

struct ioapic *ioapic;

void init_ioapic() {
    ioapic = malloc(sizeof(struct ioapic));
}

void emulate_ioapicw(struct vcpu *vcpu) {
    int offset = vcpu->kvm_run->mmio.phys_addr - IOAPIC_BASE;
    int i;

    switch (offset) {
    case 0:
        for (i = 0; i < 4; i++)
            ioapic->ioregsel |= (u32)vcpu->kvm_run->mmio.data[i] << i*8;
        break;
    case 4:
        for (i = 0; i < 4; i++) 
            ioapic->iowin |= (u32)vcpu->kvm_run->mmio.data[i] << i*8;
        /* write redirtb */
        int offset = ioapic->ioregsel - 10;
        if ((offset / 2) == 0)  {
            for (i = 0; i < 4; i++) {
                ioapic->redirtb[offset / 2].regs.lower 
                    |= (u32)vcpu->kvm_run->mmio.data[i] << i*8;
            }
        } else {
            for (i = 0; i < 4; i++) {
                ioapic->redirtb[(offset - 1) / 2].regs.upper 
                    |= (u32)vcpu->kvm_run->mmio.data[i] << i*8;
            }
        }
        break;
    default:
        break;
    }
}