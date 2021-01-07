#include "blk.h"

extern struct lapic *lapic;

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

void update_blk_index(struct blk *blk) {
    u32 index = 0 | blk->lba_low_reg | (blk->lba_middle_reg << 8) | (blk->lba_high_reg << 16);
    blk->index = index * 512;
    if (blk->drive_head_reg == 0xf0) {
        blk->index += IMGE_SIZE;
    }
}

void emulate_disk_portw(struct vcpu *vcpu, 
                        struct blk *blk, struct io io) {
    u8 val1;
    u16 val2;

    val1 = *(u8*)((u8*)vcpu->kvm_run + io.data_offset);
    
    switch (io.port) {
    case 0x1F0:
        emulate_diskw(vcpu, blk, io);
        break;
    case 0x1F2:
        blk->sec_count_reg = val1;
        break;
    case 0x1F3:
        blk->lba_low_reg = val1;
        break;            
    case 0x1F4:
        blk->lba_middle_reg = val1;
        break;
    case 0x1F5:
        blk->lba_high_reg = val1;
    case 0x1F6:
        blk->drive_head_reg = val1;
        break;
    case 0x3F6:
        blk->dev_conotrl_regs = val1;
    default:
        break;
    }

    update_blk_index(blk);

    if ((io.port == 0x1F0 || io.port == 0x1F7) && blk->dev_conotrl_regs == 0) {
        enq_irr(lapic->irr,IRQ_BASE+14);
        vcpu->kvm_run->request_interrupt_window = 1;
    }
}

void emulate_disk_portr(struct vcpu *vcpu,
                        struct blk *blk) {
    u32 data = 0;

    switch (vcpu->kvm_run->io.port) {
    case 0x1F0:
        emulate_diskr(vcpu, blk);
        break;
    case 0x1F7:
         *(unsigned char*)((unsigned char*)vcpu->kvm_run + vcpu->kvm_run->io.data_offset) = blk->status_command_reg; 
        break;
    default:
        break;
    }
}