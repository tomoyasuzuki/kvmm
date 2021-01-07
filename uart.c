#include "uart.h"

extern int outfd;

void emulate_uart_portw(struct vcpu *vcpu, struct io io, struct uart *uart) {
    switch (io.port) {
    case 0x3f8:
        for (int i = 0; i < io.count; i++) {
            char *v = (char*)((unsigned char*)vcpu->kvm_run + io.data_offset);
            printf("%c", *v);
            write(outfd, v, 1);
            uart->data_reg = *v;
            vcpu->kvm_run->io.data_offset += io.size;
        }
        break;
    case 0x3f9:
        uart->irr_enable_reg = *(u8*)((u8*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset);
    case 0x3fd:
        uart->line_status_reg = *(u8*)((u8*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset);
        break;
    default:
        break;
    }
}

void emulate_uart_portr(struct vcpu *vcpu, struct io io, struct uart *uart) {
    switch (io.port)
    {
    case 0x3f8:
        *(unsigned char*)((unsigned char*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset) = uart->data_reg; 
        break;
    case 0x3fd:
        *(unsigned char*)((unsigned char*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset) = uart->line_status_reg; 
        break;
    default:
        break;
    }
}