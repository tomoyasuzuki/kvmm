#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include "debug.h"

union dr7 {
    u32 control;
    struct {
        u32 l0 : 1;
        u32 g0 : 1;
        u32 l1 : 1;
        u32 g1 : 1;
        u32 l2 : 1;
        u32 g2 : 1;
        u32 l3 : 1;
        u32 g3 : 1;
        u32 le : 1;
        u32 ge : 1;
        u32 reserved1 : 3;
        u32 gd : 1;
        u32 reserved2 : 2;
        u32 rw0 : 2;
        u32 len0 : 2;
        u32 rw1 : 2;
        u32 len1 : 2;
        u32 rw2 : 2;
        u32 len2 : 2;
        u32 rw3 : 2;
        u32 len3 : 2;
    } bits;
};

union dr6 {
    u32 control;
    struct {
        u32 b0 : 1;
        u32 b1: 1;
        u32 b2: 1;
        u32 b3: 1;
        u32 reserved1 : 8;
        u32 reserved1sub : 1;
        u32 bd : 1;
        u32 bs : 1;
        u32 bt : 1;
        u32 reserved2 : 16;
    } bits;
};

struct sym_en {
    u32 addr;
    char *name;
};

struct debug_info {
    int bp_count;
    int status;
    void *sym_table;
    int sym_table_count;
};

struct kvm_guest_debug *debug;
union dr6 dr6;
union dr7 dr7;
unsigned long long dr0;
struct debug_info *info;

void update_debug_reg(int fd, struct kvm_guest_debug *debug);

void enable_breakpoint() {
    debug->control |= KVM_GUESTDBG_ENABLE;
    debug->control |= KVM_GUESTDBG_USE_HW_BP;
}

void disable_breakpoint() {
    debug->control &= ~(KVM_GUESTDBG_ENABLE);
    debug->control &= ~(KVM_GUESTDBG_USE_HW_BP);
}

void register_symtable_num(int num) {
    info->sym_table_count = num;
    info->sym_table = malloc(sizeof(struct sym_en) * num);
}

void register_sym_en(u32 addr, char *name, int index) {
    int size = sizeof(struct sym_en);
    struct sym_en *e = (struct sym_en*)(info->sym_table + size * index);
    e->name = malloc(100);
    memcpy(e->name, name, 100);
    e->addr = addr;
}

u32 get_func_addr(char *name) {
    for (int i = 0; i < info->sym_table_count; i++) {
        struct sym_en* e = (struct sym_en*)(info->sym_table + (int)(sizeof(struct sym_en)) * i);
        // TODO: fix Segmentation Fault when name != e->name
        if (!strcmp(name, e->name)) {
            return e->addr;
        }
    }
    return 0xffffffff;
}

void init_debug_registers(int fd) {
    info = malloc(sizeof(struct debug_info));
    info->bp_count = 0;
    info->status = 0;
    debug = malloc(sizeof(struct kvm_guest_debug));
    debug->control = 0;
    debug->pad = 0;
    dr7.control = 0;
    dr7.bits.reserved1 = 1;
    dr7.bits.reserved2= 0;
    dr7. bits.l0 = 1;
    dr7.bits.len0 = 0;
    dr7.bits.g0 = 1; // first grobal breakpoints
    dr7.bits.rw0 = 0; // only execution
    dr6.control = 0xFFFFFFFF;
    dr6.bits.b0 = 0;
    dr6.bits.b1 = 0;
    dr6.bits.b2 = 0;
    dr6.bits.b3 = 0;
    dr6.bits.reserved1 = 0xFF;
    dr6.bits.reserved1sub = 0;
    dr6.bits.bd = 0;
    dr6.bits.bs = 0;
    dr6.bits.bt = 0;
    dr0 = 0x0; // break address
    debug->arch.debugreg[0] = (unsigned long long)dr0;
    debug->arch.debugreg[6] = (unsigned long long)dr6.control;
    debug->arch.debugreg[7] = (unsigned long long)dr7.control;

    update_debug_reg(fd, debug);
}

void update_debug_reg(int fd, struct kvm_guest_debug *debug) {
    if (ioctl(fd, KVM_SET_GUEST_DEBUG, debug) < 0)
    error("KVM_SET_GUEST_DEBUG");
}

void clean_debug_reg(int fd) {
    debug->arch.debugreg[0] = 0;
    info->bp_count--;
    
    if (!info->bp_count) {
        disable_breakpoint();
    }
    update_debug_reg(fd, debug);
}

void add_breakpoint(int fd, u64 addr) {
    if (!info->status) {
        enable_breakpoint();
    }

    debug->arch.debugreg[0] = addr;
    info->bp_count++;
    update_debug_reg(fd, debug);
    printf("Breakpoint: 0x%lx\n", addr);
}

void handle_debug(struct vcpu *vcpu) {
    if (ioctl(vcpu->fd, KVM_GET_REGS, &(vcpu->regs)) < 0)
        return;

    printf("Stop at 0x%llx\n", vcpu->regs.rip);
    
    // NOTE: When below function removed, vm stop at the bp eternally.
    clean_debug_reg(vcpu->fd);
}