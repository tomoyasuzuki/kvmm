#include "blk.h"

void emulate_diskr(struct vcpu *vcpu, struct blk *blk) {
    u32 data = 0;
    for (int i = 0; i < vcpu->kvm_run->io.count; ++i) {
        for (int j = 0; j < 4; j++) {
            data |= blk->data[blk->index] << (8 * j);
            blk->index += 1;
        }
        *(u32*)((unsigned char*)vcpu->kvm_run + vcpu->kvm_run->io.data_offset) = data;
        vcpu->kvm_run->io.data_offset += vcpu->kvm_run->io.size;
        data = 0;
    }
}

void emulate_diskw(struct vcpu *vcpu, 
                   struct blk *blk, struct io io) {
    for (int i = 0; i < io.count; i++) {
        u32 val4 = *(u32*)((u32*)vcpu->kvm_run + io.data_offset);
        blk->data[blk->index] = val4;
        vcpu->kvm_run->io.data_offset += io.size;
        blk->index += io.size;
    }
}