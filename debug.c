#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
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

struct kvm_guest_debug *debug;
union dr6 dr6;
union dr7 dr7;
unsigned long long dr0;

 void update_debug_reg(int fd, struct kvm_guest_debug *debug);

 void init_debug_registers(int fd) {
    debug = malloc(sizeof(struct kvm_guest_debug));
    debug->control = 0;
    debug->pad = 0;
    debug->control |= KVM_GUESTDBG_ENABLE;
    debug->control |= KVM_GUESTDBG_USE_HW_BP;
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
     debug->control &= ~(KVM_GUESTDBG_ENABLE);
     debug->control &= ~(KVM_GUESTDBG_USE_HW_BP);

     update_debug_reg(fd, debug);
 }

 void add_breakpoint(int fd, u64 addr) {
     debug->arch.debugreg[0] = addr;
     update_debug_reg(fd, debug);
     printf("Breakpoint: 0x%lx\n", addr);
 }

 void handle_debug(struct vcpu *vcpu) {
     if (ioctl(vcpu->fd, KVM_GET_REGS, &(vcpu->regs)) < 0)
        return;

     printf("Stop at 0x%llx\n", vcpu->regs.rip);

     clean_debug_reg(vcpu->fd);
 }