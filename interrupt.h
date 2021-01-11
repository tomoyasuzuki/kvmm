// #pragma once

// #include <linux/kvm.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <fcntl.h>
// #include <sys/ioctl.h>
// #include "vcpu.h"

// struct irr_queue {
//     int buff[1000];
//     int last;
// };

// void emulate_interrupt(struct vcpu *vcpu);
// void enq_irr(struct irr_queue *irr, int value);
// int deq_irr(struct irr_queue *irr);
// void inject_interrupt(int vcpufd, int irq);

// struct interrupt_buffer {
//     int head, end, count, max;
//     int *buff;
// };