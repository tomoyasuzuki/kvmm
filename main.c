#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>

#define CODE_START 0x0
#define GUEST_PATH "../xv6/xv6.img"
#define START_ADDRESS 0x7c00
#define GUEST_MEMORY_SIZE 0x80000000
#define ALIGNMENT_SIZE 0x1000
#define DEFAULT_FLAGS 0x0000000000000002ULL
#define GUEST_BINARY_SIZE (4096 * 128)
#define IMGE_SIZE 5120000
#define LAPIC_BASE 0xfee00000
#define IOAPIC_BASE 0xfec00000
#define IOAPIC_REDRTB_BASE (IOAPIC_BASE + 0x10)
#define MSR_IA32_APICBASE 0x0000001b

typedef uint8_t u8;
typedef uint8_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef struct kvm_userspace_memory_region kvm_mem;

struct vm {
    int vm_fd;
    int fd;
    void *mem;
};

struct vcpu {
    int fd;
    struct kvm_run *kvm_run;
    struct kvm_sregs sregs;
    struct kvm_regs regs;
};

struct kvm_lapic;

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

struct io {
    __u8 direction;
    __u8 size;
    __u16 port;
    __u32 count;
    __u64 data_offset; 
};

struct mmio {
    __u64 phys_addr;
	__u8  data[8];
	__u32 len;
	__u8  is_write;
};

struct uart {
    u8 data_reg;
    u8 irr_enable_reg;
    u8 irr_id_reg;
    u8 line_control_reg;
    u8 modem_control_reg;
    u8 line_status_reg;
    u8 modem_status_reg;
    u8 scratch_reg;
};

union redirtb_entry {
    struct {
        u64 vec : 8;
        u64 deliverly_mode : 3;
        u64 dest_mode : 1;
        u64 deliverly_status : 1;
        u64 pin_polarity : 1;
        u64 remote_irr : 1;
        u64 trigger_mode : 1;
        u64 mask : 1;
        u64 : 39;
        u64 destination : 8;
    } fields;
    struct {
        u32 lower;
        u32 upper;
    } regs;
};

struct ioapic {
    u32 ioregsel;
    u32 iowin;
    u32 id;
    u32 vec;
    union redirtb_entry redirtb[24];
};

struct irr_queue {
    int arr[1000];
    int last;
};

struct lapic {
    u32 regs[1024];
    struct irr_queue *irr;
};

int outfd = 0;

struct vm *vm;
struct vcpu *vcpu;
struct lapic *lapic;

void enq_irr(struct irr_queue *irr, int value) {
    printf("enq %d\n", value);
    irr->arr[irr->last] = value;
    irr->last++;
}

int deq_irr(struct irr_queue *irr) {
   int out = irr->arr[0];
   for (int i = 0; i <= irr->last; i++) {
       irr->arr[i] = irr->arr[i+1];
   }
   irr->last--;
}   

void error(char *message) {
    perror(message);
    exit(1);
}

void inject_interrupt(int vcpufd, int irq) {
    struct kvm_interrupt *intr = malloc(4096);
    intr->irq = irq;
    
    
    if (ioctl(vcpufd, KVM_INTERRUPT, intr) < 0)
        perror("KVM_INTERRUPT");
}

void init_vcpu(struct vm *vm, struct vcpu *vcpu) {
    int mmap_size;

    vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
    
    if (vcpu->fd < 0) {
        error("KVM_CREATE_VCPU");
    }

    mmap_size = ioctl(vm->vm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);

    if (mmap_size <= 0) {
        error("KVM_GET_VCPU_MMAP_SIZE");
    }

    vcpu->kvm_run = mmap(NULL, mmap_size, 
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED, vcpu->fd, 0);

    if (vcpu->kvm_run == MAP_FAILED) {
        error("kvm_run: failed\n");
    }
}

void set_sregs(struct kvm_sregs *sregs) {
    sregs->cs.selector = 0;
	sregs->cs.base = 0;
}

void set_regs(struct vcpu *vcpu) {
    if (ioctl(vcpu->fd, KVM_GET_SREGS, &(vcpu->sregs)) < 0) {
        error("KVM_GET_SREGS");
    }

    set_sregs(&vcpu->sregs);

    if (ioctl(vcpu->fd, KVM_SET_SREGS, &(vcpu->sregs)) < 0) {
        error("KVM_SET_SREGS");
    }

    vcpu->regs.rflags = DEFAULT_FLAGS;
    vcpu->regs.rip = START_ADDRESS;

    if (ioctl(vcpu->fd, KVM_SET_REGS, &(vcpu->regs)) < 0) {
        error("KVM_SET_REGS");
    }
}

void create_lapic(struct kvm_lapic_state *lapic) {
    if (ioctl(vm->vm_fd, KVM_CHECK_EXTENSION, 121) <= 0)
        error("KVM_CHECK_EXTENSION");
    
    // if (ioctl(vm->fd, KVM_CAP_CHECK_EXTENSION_VM, 121) <= 0)
    //     error("KVM_CAP_CHECK_EXTENSION_VM");

    printf("KVM_CAP_SPLIT_IRQCHIP is avaiable\n");

    struct kvm_enable_cap *cap = malloc(4096);
    cap->cap = 121; // = KVM_CAP_PLIT_IRQCHIP
    cap->flags = 0;
    cap->args[0] = 24; // = num of ioapic pin

    if (ioctl(vm->fd, KVM_ENABLE_CAP, cap) < 0)
        error("KVM_ENABLE_CAP");

    
    
    // if (ioctl(vcpu->fd, KVM_GET_LAPIC, lapic) < 0)
    //     error("KVM_GET_LAPIC at create_lapic");

    printf("success to get lapic\n");
}

void load_guest_binary(void *dst) {
    int fd = open("../xv6/bootblock", O_RDONLY);
    if (fd < 0) 
        error("open fail");
    
    void *tmp = dst;
    for(;;) {
        int size = read(fd, tmp + START_ADDRESS, 
                        GUEST_BINARY_SIZE);
        if (size <= 0) 
            break;
        tmp += size;
    }
}

void memalign(void **dst, size_t size, size_t align) {
    if (posix_memalign(dst, size, align) < 0) {
        printf("memalign: faile\n");
        exit(1);
    }
}

void set_vm_mem(struct vm *vm, 
                            kvm_mem *memreg,
                            u64 phys_start,
                            size_t size) {

    memalign(&(vm->mem), size, ALIGNMENT_SIZE);

    memreg->slot = 0;
	memreg->flags = 0;
	memreg->guest_phys_addr = phys_start;
	memreg->memory_size = (u64)size;
	memreg->userspace_addr = (unsigned long)vm->mem;
    
    if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, memreg) < 0) {
		error("KVM_SET_USER_MEMORY_REGION");
	}
}

void print_regs(struct vcpu *vcpu) {
    ioctl(vcpu->fd, KVM_GET_REGS, &(vcpu->regs));
    printf("rip: 0x%llx\n", vcpu->regs.rip);
}

void create_blk(struct blk *blk) {
    blk->data = malloc(IMGE_SIZE);
    blk->data_reg = 0;
    blk->drive_head_reg = 0;
    blk->lba_high_reg = 0;
    blk->lba_middle_reg = 0;
    blk->lba_low_reg = 0;
    blk->sec_count_reg = 0;
    blk->status_command_reg = 0x40;
    blk->dev_conotrl_regs = 0; 

    int img_fd = open("../xv6/xv6.img", O_RDONLY);
    if (img_fd < 0) {
        perror("fail open xv6.img");
        exit(1);
    }

    void *tmp = (void*)blk->data;
    for(;;) {
        int size = read(img_fd, tmp, IMGE_SIZE);
        if (size <= 0) 
            break;

        tmp += size;
    }
}

void init_kvm(struct vm *vm) {
    vm->vm_fd = open("/dev/kvm", O_RDWR);
    if (vm->vm_fd < 0) { 
        error("open /dev/kvm");
    }
}

void create_vm(struct vm *vm) {
    vm->fd = ioctl(vm->vm_fd, KVM_CREATE_VM, 0);
    if (vm->fd < 0) {
        error("KVM_CREATE_VM");
    }
}

void create_pit(int fd, struct kvm_pit_state2 *pit) {
    // pit channel 0 is connected directly to IRQ0
    struct kvm_pit_config config = {
        .flags = 1,
        .pad = 0
    };

    if (ioctl(fd, KVM_CREATE_PIT2, &config) < 0)
        error("KVM_CREATE_PIT2");

    if (ioctl(fd, KVM_GET_PIT2, pit) < 0)
        error("KVM_GET_PIT2");
    
    pit->channels[0].count = 65535;

    if (ioctl(fd, KVM_SET_PIT2, pit) < 0)
        error("KVM_SET_PIT2");
}

void create_uart(struct uart *uart) {
    uart->data_reg = 0;
    uart->irr_enable_reg = 0;
    uart->irr_id_reg = 0;
    uart->line_control_reg = 0;
    uart->modem_control_reg = 0;
    uart->line_status_reg = 0;
    uart->modem_status_reg = 0;
    uart->scratch_reg = 0;
}

void set_tss(int fd) {
    if (ioctl(fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
        error("KVM_SET_TSS_ADDR");
    }
}

void create_output_file() {
    outfd = open("out.txt", O_RDWR | O_CREAT);
}

void set_lapicbase(struct vcpu *vcpu, struct kvm_msrs *msrs) {
    if (ioctl(vcpu->fd, KVM_GET_MSRS, msrs) < 0)
        error("KVM_GET_MSRS at set_lapicbase");

    if (msrs->entries[MSR_IA32_APICBASE].data == LAPIC_BASE)
        return; // lapic address is already set

    printf("change lapic_base from 0x%llx to 0xfee00000\n", 
            msrs->entries[MSR_IA32_APICBASE].data);
    msrs->entries[MSR_IA32_APICBASE].data = LAPIC_BASE;

    if (ioctl(vcpu->fd, KVM_SET_MSRS, msrs) < 0)
        error("KVM_SET_MSRS at set_lapicbase");
}

void set_ioapicbase(struct vm *vm, struct kvm_irqchip *irq) {
    if (ioctl(vm->fd, KVM_GET_IRQCHIP, irq) < 0)
        error("KVM_GET_IRQCHIP at set_ioapicbase");
    
    if (irq->chip.ioapic.base_address == IOAPIC_BASE)
        return;
    
    irq->chip.ioapic.base_address = IOAPIC_BASE;

    if (ioctl(vm->fd, KVM_SET_IRQCHIP, irq) < 0)
        error("KVM_SET_IRQCHIP at set_ioapicbase");
}

void emulate_diskr(struct blk *blk) {
    u32 i = 0 | blk->lba_low_reg | (blk->lba_middle_reg << 8) | (blk->lba_high_reg << 16) |
            ((blk->drive_head_reg & 0x0F) << 24);
    blk->index = i * 512;
}

void emulate_disk_portw(struct vcpu *vcpu, 
                        struct blk *blk, struct io io) {
    u8 val1;
    u16 val2;

    //TODO: check io size
    //if (io.size !=  (1 | 2))
        //return;
    
    if (io.port == 0x1F0) {
        for (int i = 0; i < io.count; i++) {
            val1 = *(u8*)((u8*)vcpu->kvm_run + io.data_offset);
            blk->data[blk->index] = val1;
            vcpu->kvm_run->io.data_offset += io.size;
            blk->index += 1;
        }

        if (blk->dev_conotrl_regs == 0) {
            enq_irr(lapic->irr,32+14);
        } else {
            vcpu->kvm_run->request_interrupt_window = 1;         
        }

        return;
    }

    val1 = *(u8*)((u8*)vcpu->kvm_run + io.data_offset);
    
    switch (io.port) {
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
    case 0x1F7:
        if (val1 == 0x20) {
            emulate_diskr(blk);

            if (blk->dev_conotrl_regs == 0) {
                enq_irr(lapic->irr,32+14);
            } else {
                vcpu->kvm_run->request_interrupt_window = 1;
            }
        }

        break;
    case 0x3F6:
        blk->dev_conotrl_regs = val1;
    default:
        break;
    }
}

void emulate_disk_portr(struct vcpu *vcpu,
                        struct blk *blk) {
    u32 data = 0;

    switch (vcpu->kvm_run->io.port) {
    case 0x1F0:
        for (int i = 0; i < vcpu->kvm_run->io.count; ++i) {
            for (int j = 0; j < 4; j++) {
                data |= blk->data[blk->index] << (8 * j);
                blk->index += 1;
            }
            *(u32*)((unsigned char*)vcpu->kvm_run + vcpu->kvm_run->io.data_offset) = data;
            vcpu->kvm_run->io.data_offset += vcpu->kvm_run->io.size;
            data = 0;
        }
        break;
    case 0x1F7:
         *(unsigned char*)((unsigned char*)vcpu->kvm_run + vcpu->kvm_run->io.data_offset) = blk->status_command_reg; 
        break;
    default:
        break;
    }
}

void emulate_uart_portw(struct vcpu *vcpu, struct io io, struct uart *uart) {
    switch (io.port) {
    case 0x3f8:
        for (int i = 0; i < io.count; i++) {
            char *v = (char*)((unsigned char*)vcpu->kvm_run + io.data_offset);
            write(outfd, v, 1);
            uart->data_reg = *v;
            vcpu->kvm_run->io.data_offset += io.size;
        }
        //uart->line_status_reg |= (1<<0);
        //if (uart->irr_enable_reg) 
            //enq_irr(lapic->irr, 32+4);
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
        //uart->line_status_reg &= ~(1<<0);
        //if (uart->irr_enable_reg == 0xff) 
            //enq_irr(lapic->irr, 32+4);
        break;
    case 0x3fd:
        *(unsigned char*)((unsigned char*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset) = uart->line_status_reg; 
        break;
    default:
        break;
    }
}

void emulate_pic_portw(struct vcpu *vcpu, struct io io, 
                        struct kvm_irqchip *irq) {
    
    if (ioctl(vm->fd, KVM_GET_IRQCHIP, irq) < 0)
        error("KVM_GET_IRQCHIP at emulate_pic_portw");

    irq->chip.pic.init_state = *(char*)((char*)vcpu->kvm_run + io.data_offset);
    vcpu->kvm_run->io.data_offset += io.size;

    if (ioctl(vm->fd, KVM_SET_IRQCHIP, irq) < 0)
        error("KVM_SET_IRQCHIP at emulate_pic_portw");
    
    if (ioctl(vm->fd, KVM_GET_IRQCHIP, irq) < 0)
        error("KVM_GET_IRQCHIP at emulate_pic_portw");

    printf("pic init_state: 0x%x\n", irq->chip.pic.init_state);
    exit(1);
    return;
}

void emulate_io_out(struct vcpu *vcpu, struct blk *blk, 
                    struct io io, struct uart *uart) {
    switch (io.port) {
    case 0x1F0 ... 0x1F7:
        emulate_disk_portw(vcpu, blk, io);
        break;
    case 0x3F6:
        emulate_disk_portw(vcpu, blk, io);
        break;
    case 0x3F8 ... 0x3FD:
        emulate_uart_portw(vcpu, io, uart);
        break;
    default:
        break;
    }
}

void emulate_io_in(struct vcpu *vcpu, struct blk *blk, 
                   struct io io, struct uart *uart) {
    switch (io.port) {
    case 0x1F0 ... 0x1F7:
        emulate_disk_portr(vcpu, blk);
        break;
    case 0x3f8 ... 0x3fd:
        emulate_uart_portr(vcpu, io, uart);
    default:
        break;
    }
}

void emulate_io(struct vcpu *vcpu, struct blk *blk, struct uart *uart) {
    struct io io = {
        .direction = vcpu->kvm_run->io.direction,
        .size = vcpu->kvm_run->io.size,
        .port = vcpu->kvm_run->io.port,
        .count = vcpu->kvm_run->io.count,
        .data_offset = vcpu->kvm_run->io.data_offset
    };

    //printf("port: 0x%x\n", io.port);

    switch (io.direction) {
    case KVM_EXIT_IO_OUT:
        //printf("out: %d\n", io.port);
        //print_regs(vcpu);
        emulate_io_out(vcpu, blk, io, uart);
        break;
    case KVM_EXIT_IO_IN:
        //printf("in: %d\n", io.port);
        //print_regs(vcpu);
        emulate_io_in(vcpu, blk, io, uart);
        break;
    default:
        printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
        print_regs(vcpu);
        break;
    }
}

void emulate_lapicw(struct vcpu *vcpu, struct lapic *lapic) {
    int index = vcpu->kvm_run->mmio.phys_addr - LAPIC_BASE;
    u32 data = 0;

    for (int i = 0; i < 4; i++) {
        data |= vcpu->kvm_run->mmio.data[i] << i*8;
    }

    //printf("lapic offset=%d, val=%d\n", index/4, data);

    if (vcpu->kvm_run->mmio.is_write)
        lapic->regs[index/4] = data;
}

/*
    emulate ioapic access here. 

    1. calculate redirtb offset from phys addr
    2. check offset is IOREGSEL or IOWIN 
*/

void emulate_ioapicw(struct vcpu *vcpu, struct ioapic *ioapic) {
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

void emulate_mmio(struct vcpu *vcpu, struct vm *vm,
                  struct lapic *lapic,
                  struct ioapic *ioapic) {
    struct mmio mmio = {
        .data = vcpu->kvm_run->mmio.data,
        .is_write = vcpu->kvm_run->mmio.is_write,
        .len = vcpu->kvm_run->mmio.len,
        .phys_addr = vcpu->kvm_run->mmio.phys_addr
    };

    if (mmio.phys_addr >= LAPIC_BASE)
        emulate_lapicw(vcpu, lapic);
    if (mmio.phys_addr >= IOAPIC_BASE) {
        emulate_ioapicw(vcpu, ioapic);
    }
}

void debug_msrs(struct vcpu *vcpu, struct kvm_msrs *msrs) {
    if (ioctl(vcpu->fd, KVM_GET_MSRS, msrs) < 0)
        error("KVM_GET_MSRS at debug_msrs");
    
    printf("apic base: 0x%llx\n", msrs->entries[MSR_IA32_APICBASE].data);
}

void debug_irq_status(struct vm *vm, struct kvm_irqchip *irq) {
    if (ioctl(vm->fd, KVM_GET_IRQCHIP, irq) < 0) 
        error("KVM_GET_IRQCHIP");
    //printf("irq id: 0x%x\n", irq->chip_id);
    printf("ioapic base: 0x%llx\n", irq->chip.ioapic.base_address);

    irq->chip.ioapic.base_address = IOAPIC_BASE;
    irq->chip_id = 2;

    if (ioctl(vm->fd, KVM_SET_IRQCHIP, irq) < 0)
        error("KVM_SET_IRQCHIP at debug_irq_status");
    for (int i = 0; irq->chip.ioapic.redirtbl[i].bits; i++) {
        printf("irq %d: derivery status 0x%x\n", 
                irq->chip.ioapic.redirtbl[i].fields.vector, 
                irq->chip.ioapic.redirtbl[i].fields.delivery_mode);
    }
}

void debug_lapic_status(struct vcpu *vcpu, struct kvm_lapic_state *lapic) {
    if (ioctl(vcpu->fd, KVM_GET_LAPIC, lapic) < 0)
        error("KVM_GET_LAPIC");
    printf("lapic version: 0x%x\n", lapic->regs[4 * 12]);
    printf("lapic ticr: 0x%x\n", lapic->regs[4 * 224]);
}

void debug_sregs(struct vcpu *vcpu) {
    if (ioctl(vcpu->fd, KVM_GET_SREGS, &(vcpu->sregs)) < 0)
        error("KVM_GET_SREGS");
    /* 
        vcpu->sregs.apic_base is only valid 
        if in-kernel local APIC is not used
    */
    printf("apic base: 0x%llx\n", vcpu->sregs.apic_base);
    printf("idt base: 0x%llx\n", vcpu->sregs.idt.base);
}

void debug_vcpu_events(struct vcpu *vcpu, struct kvm_vcpu_events *events) {
    if (ioctl(vcpu->fd, KVM_GET_VCPU_EVENTS, events) < 0)
        error("KVM_GET_VCPU_EVENTS");
    printf("interrupt.injected = 0x%x\n", events->interrupt.injected);
    printf("interrupt.nr = 0x%x\n", events->interrupt.nr);
    printf("interrupt.soft = 0x%x\n", events->interrupt.soft);
    printf("interrupt.shadow = 0x%x\n", events->interrupt.shadow);
}

void debug_pit(struct vm *vm, struct kvm_pit_state2 *pit) {
    if (ioctl(vm->fd, KVM_GET_PIT2, pit) < 0)
        error("KVM_GET_PIT2");
    printf("pit0.read_state 0x%x\n", pit->channels[0].read_state);
    printf("pit0.write_state 0x%x\n", pit->channels[0].write_state);
    printf("pit0.count 0x%x\n", pit->channels[0].count);
}


int main(int argc, char **argv) {
    vm = malloc(sizeof(struct vm));
    vcpu = malloc(sizeof(struct vcpu));
    lapic = malloc(sizeof(struct lapic));
    lapic->irr = malloc(4 * 4096);
    for (int j = 0; j < 1000; j++) {
        lapic->irr->arr[j] = -1;
    }

    kvm_mem *memreg = malloc(sizeof(kvm_mem));
    struct blk *blk = malloc(sizeof(struct blk));
    struct kvm_vcpu_events *events = malloc(sizeof(struct kvm_vcpu_events));
    struct kvm_pit_state2 *pit = malloc(sizeof(struct kvm_pit_state2));
    struct kvm_msrs *msrs = malloc(sizeof(struct kvm_msrs));
    struct ioapic *ioapic = malloc(sizeof(struct ioapic));
    struct uart *uart = malloc(sizeof(struct uart));

    init_kvm(vm);
    create_vm(vm);
    create_pit(vm->fd, pit);
    create_blk(blk);
    set_tss(vm->fd);
    set_vm_mem(vm, memreg, 0, GUEST_MEMORY_SIZE);    
    load_guest_binary(vm->mem);
    init_vcpu(vm, vcpu);
    set_regs(vcpu);
    create_uart(uart);
    create_output_file();

    for (;;) {
        if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
            print_regs(vcpu);
            error("KVM_RUN");
        }

        struct kvm_run *run = vcpu->kvm_run;

        int i = 0;

        switch (run->exit_reason) {
        case KVM_EXIT_IO:
            print_regs(vcpu);
            emulate_io(vcpu, blk, uart);
            // if (i / 1000 == 0)
            //     enq_irr(lapic->irr, 32+0);
            i++;
            break;
        case KVM_EXIT_MMIO:
            emulate_mmio(vcpu, vm, lapic, ioapic);
            break;
        case KVM_EXIT_EXCEPTION:
            printf("exception\n");
        case KVM_EXIT_IRQ_WINDOW_OPEN:
            if (lapic->irr->arr[0] > 32) {
                inject_interrupt(vcpu->fd, lapic->irr->arr[0]);
                printf("inject %d\n", lapic->irr->arr[0]);
                deq_irr(lapic->irr);
                vcpu->kvm_run->request_interrupt_window = 0;
            }
            break;
        default:
            printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
            break;
        }
    }
    return 1;   
}