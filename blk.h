#include "type.h"
#include "vcpu.h"

struct blk {
    u8 *data;
    u16 data_reg;
    u8 sec_count_reg;
    u8 lba_low_reg;
    u8 lba_middle_reg;
    u8 lba_high_reg;
    u8 drive_head_reg;
    u8 status_command_reg;
    u32 index;
    u8 dev_conotrl_regs;
};

void emulate_diskr(struct vcpu *vcpu, struct blk *blk);
