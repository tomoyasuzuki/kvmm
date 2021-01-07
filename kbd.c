#include "kbd.h"

extern struct vcpu *vcpu;

void emulate_kbd_portr(struct vcpu *vcpu) {
    switch (vcpu->kvm_run->io.port)
    {
    case 0x64:
        *(unsigned char*)((unsigned char*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset) = 1;
        break;
    case 0x60:
        // *(unsigned char*)((unsigned char*)vcpu->kvm_run
        //  + vcpu->kvm_run->io.data_offset) = vk;
        break;
    default:
        break;
    }
}