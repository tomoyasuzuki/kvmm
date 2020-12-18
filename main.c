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
    u8 line_status_reg;
};

int outfd = 0;

struct vm *vm;
struct vcpu *vcpu;

void error(char *message) {
    perror(message);
    exit(1);
}

void inject_interrupt(int vcpufd, int irq) {
    struct kvm_interrupt *intr = malloc(4096);
    intr->irq = irq;
    
    
    if (ioctl(vcpufd, KVM_INTERRUPT, intr) < 0)
        error("KVM_INTERRUPT");
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

void create_irqchip(int fd, struct kvm_irqchip *irq) {
    // struct kvm_irq_routing *rout = malloc(4096);
    // rout->nr = 0;

    // for (int i = 0; i < 15; i++) {
    //     rout->nr++;
    //     rout->flags = 0;
    //     rout->entries[i].gsi = i;
    //     rout->entries[i].type = KVM_IRQ_ROUTING_IRQCHIP;
    //     rout->entries[i].flags = 0;
    //     rout->entries[i].u.irqchip.irqchip = KVM_IRQCHIP_IOAPIC;
    //     rout->entries[i].u.irqchip.pin = i; 
    // }

    // if (ioctl(fd, KVM_SET_GSI_ROUTING, rout) < 0)
    //     error("KVM_SET_GSI_ROUTING");
    
    if (ioctl(fd, KVM_CREATE_IRQCHIP, 0) < 0)
        error("KVM_CREATE_IRQCHIP");

    if (ioctl(fd, KVM_GET_IRQCHIP, irq) < 0)
        error("KVM_GET_IRQCHIP at create_irqchip");
    
    irq->chip.ioapic.base_address = IOAPIC_BASE;
    irq->chip_id = 2;

    if (ioctl(fd, KVM_SET_IRQCHIP, irq) < 0)
        error("KVM_SET_LAPIC at create_irqchip");
    
    // for (int i = 0; i < 15; i++) {
    //     struct kvm_irq_level *level = malloc(sizeof(struct kvm_irq_level)); 
    //     level->irq = i;
    //     level->level = 1;
    //     if (ioctl(fd, KVM_IRQ_LINE, level) < 0)
    //         error("KVM_IRQ_LINE");
    // }

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

    printf("vm");
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
        print_regs(vcpu);
        printf("io size %d count %d\n", io.size, io.count);
        for (int i = 0; i < io.count; i++) {
            val1 = *(u8*)((u8*)vcpu->kvm_run + io.data_offset);
            blk->data[blk->index] = val1;
            vcpu->kvm_run->io.data_offset += io.size;
            blk->index += 1;
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
        }

        // nIEN bit is clear. interrupts is enabled.
        // if (blk->dev_conotrl_regs == 0) {
        //     inject_interrupt(vcpu->fd, 3257);
        //     exit(1);
        // }

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

void emulate_uart_portw(struct vcpu *vcpu, struct io io) {
    switch (io.port) {
    case 0x3f8:
        for (int i = 0; i < io.count; i++) {
            char *v = (char*)((unsigned char*)vcpu->kvm_run + io.data_offset);
            write(outfd, v, 1);
            vcpu->kvm_run->io.data_offset += io.size;
        }
        break;
    default:
        break;
    }
}

void emulate_uart_portr(struct vcpu *vcpu, struct io io) {
    switch (io.port)
    {
    case 0x3fd:
        *(unsigned char*)((unsigned char*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset) = 0; 
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
                    struct io io, struct kvm_irqchip *irq) {
    switch (io.port) {
    case 0x1F0 ... 0x1F7:
        emulate_disk_portw(vcpu, blk, io);
        break;
    case 0x3F6:
        emulate_disk_portw(vcpu, blk, io);
        break;
    case 0x3F8:
        emulate_uart_portw(vcpu, io);
        break;
    case 0x21:
        emulate_pic_portw(vcpu, io, irq);
    default:
        break;
    }
}

void emulate_io_in(struct vcpu *vcpu, struct blk *blk, struct io io) {
    switch (io.port) {
    case 0x1F0 ... 0x1F7:
        emulate_disk_portr(vcpu, blk);
        break;
    case 0x3f8 ... 0x3fd:
    default:
        break;
    }
}

void emulate_io(struct vcpu *vcpu, struct blk *blk, struct kvm_irqchip *irq) {
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
        printf("out: %d\n", io.port);
        print_regs(vcpu);
        emulate_io_out(vcpu, blk, io, irq);
        break;
    case KVM_EXIT_IO_IN:
        printf("in: %d\n", io.port);
        print_regs(vcpu);
        emulate_io_in(vcpu, blk, io);
        break;
    default:
        printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
        print_regs(vcpu);
        break;
    }
}

void emulate_lapicw(struct vcpu *vcpu, struct kvm_lapic_state *lapic) {
    int index = vcpu->kvm_run->mmio.phys_addr - LAPIC_BASE;
    u32 data = 0;

    if (ioctl(vcpu->fd, KVM_GET_LAPIC, lapic) < 0) {
        error("KVM_GET_LAPIC");
    }

    for (int i = 0; i < 4; i++) {
        data |= vcpu->kvm_run->mmio.data[i] << i*8;
    }

    printf("lapic: %d\n", index);
    exit(1);

    lapic->regs[index/4] = data;
    if (ioctl(vcpu->fd, KVM_SET_LAPIC, lapic) < 0) {
        error("KVM_SET_LAPIC");
    }
}

void emulate_ioapicw(struct vcpu *vcpu, struct kvm_irqchip *irq) {
    int index = vcpu->kvm_run->mmio.phys_addr - IOAPIC_BASE;

    switch (index) {
    case 0:
        break;
    default:
        break;
    }
}

void emulate_mmio(struct vcpu *vcpu, struct vm *vm,
                  struct kvm_lapic_state *lapic,
                  struct kvm_irqchip *irq) {
    struct mmio mmio = {
        .data = vcpu->kvm_run->mmio.data,
        .is_write = vcpu->kvm_run->mmio.is_write,
        .len = vcpu->kvm_run->mmio.len,
        .phys_addr = vcpu->kvm_run->mmio.phys_addr
    };

    /* 
        because kvm handle mmio access internally, 
        this function should not be called accessing lapic or ioapic.  
    */

    if (mmio.phys_addr >= LAPIC_BASE)
        emulate_lapicw(vcpu, lapic);
    if (mmio.phys_addr >= IOAPIC_BASE) {
        if (ioctl(vm->fd, KVM_GET_IRQCHIP, irq) < 0)
            error("KVM_GET_IRQCHIP at emulate_mmio");
        emulate_ioapicw(vcpu, irq);
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
    //printf("ioapic id: 0x%x\n", irq->chip.ioapic.id);
    //printf("pic id: 0x%x\n", irq->chip.pic.id);
}

void debug_lapic_status(struct vcpu *vcpu, struct kvm_lapic_state *lapic) {
    if (ioctl(vcpu->fd, KVM_GET_LAPIC, lapic) < 0)
        error("KVM_GET_LAPIC");
    printf("lapic id: 0x%x\n", lapic->regs[8]);
    printf("lapic timer: 0x%x\n", lapic->regs[200]);
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

    struct blk *blk = malloc(sizeof(struct blk));
    struct kvm_lapic_state *lapic = malloc(sizeof(struct kvm_lapic_state));
    kvm_mem *memreg = malloc(sizeof(kvm_mem));
    struct kvm_irqchip *irq = malloc(sizeof(struct kvm_irqchip));
    struct kvm_vcpu_events *events = malloc(sizeof(struct kvm_vcpu_events));
    struct kvm_pit_state2 *pit = malloc(sizeof(struct kvm_pit_state2));
    struct kvm_msrs *msrs = malloc(sizeof(struct kvm_msrs));

    printf("hoge");

    init_kvm(vm);
    create_vm(vm);
    create_irqchip(vm->fd, irq);
    create_pit(vm->fd, pit);
    create_blk(blk);
    set_tss(vm->fd);
    set_vm_mem(vm, memreg, 0, GUEST_MEMORY_SIZE);    
    load_guest_binary(vm->mem);
    init_vcpu(vm, vcpu);
    set_regs(vcpu);
    set_lapicbase(vcpu, msrs);
    set_ioapicbase(vm, irq);
    create_output_file();

    for (;;) {
        if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
            error("KVM_RUN");
            print_regs(vcpu);
        }

        printf("run");

        struct kvm_run *run = vcpu->kvm_run;

        switch (run->exit_reason) {
        case KVM_EXIT_IO:
            print_regs(vcpu);
            emulate_io(vcpu, blk, irq);
            break;
        case KVM_EXIT_MMIO:
            emulate_mmio(vcpu, vm, lapic, irq);
            break;
        case KVM_EXIT_EXCEPTION:
            printf("exception\n");
            exit(1);
        case KVM_EXIT_INTR:
            printf("interrupt\n");
            exit(1);
        default:
            printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
            exit(1);
            break;
        }
    }
    return 1;   
}