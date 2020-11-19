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

#define CODE_START 0
#define GUEST_PATH "bootblock"

#define CR0_PE 1u
#define CR0_MP (1U << 1)
#define CR0_EM (1U << 2)
#define CR0_TS (1U << 3)
#define CR0_ET (1U << 4)
#define CR0_NE (1U << 5)
#define CR0_WP (1U << 16)
#define CR0_AM (1U << 18)
#define CR0_NW (1U << 29)
#define CR0_CD (1U << 30)
#define CR0_PG (1U << 31)

/* CR4 bits */
#define CR4_VME 1
#define CR4_PVI (1U << 1)
#define CR4_TSD (1U << 2)
#define CR4_DE (1U << 3)
#define CR4_PSE (1U << 4)
#define CR4_PAE (1U << 5)
#define CR4_MCE (1U << 6)
#define CR4_PGE (1U << 7)
#define CR4_PCE (1U << 8)
#define CR4_OSFXSR (1U << 8)
#define CR4_OSXMMEXCPT (1U << 10)
#define CR4_UMIP (1U << 11)
#define CR4_VMXE (1U << 13)
#define CR4_SMXE (1U << 14)
#define CR4_FSGSBASE (1U << 16)
#define CR4_PCIDE (1U << 17)
#define CR4_OSXSAVE (1U << 18)
#define CR4_SMEP (1U << 20)
#define CR4_SMAP (1U << 21)

#define EFER_SCE 1
#define EFER_LME (1U << 8)
#define EFER_LMA (1U << 10)
#define EFER_NXE (1U << 11)

/* 32-bit page directory entry bits */
#define PDE32_PRESENT 1
#define PDE32_RW (1U << 1)
#define PDE32_USER (1U << 2)
#define PDE32_PS (1U << 7)

/* 64-bit page * entry bits */
#define PDE64_PRESENT 1
#define PDE64_RW (1U << 1)
#define PDE64_USER (1U << 2)
#define PDE64_ACCESSED (1U << 5)
#define PDE64_DIRTY (1U << 6)
#define PDE64_PS (1U << 7)
#define PDE64_G (1U << 8)

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

struct blk {
    void *data;
    uint16_t data_reg;
    uint8_t sec_count_reg;
    uint8_t lba_low_reg;
    uint8_t lba_middle_reg;
    uint8_t lba_high_reg;
    uint8_t drive_head_reg;
    uint8_t status_command_reg;
};

void init_vcpu(struct vm *vm, struct vcpu *vcpu) {
    int vcpu_mmap_size;

    vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
    if (vcpu->fd < 0) {
        perror("KVM_CREATE_VCPU");
        exit(1);
    }

    vcpu_mmap_size = ioctl(vm->vm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (vcpu_mmap_size <= 0) {
        perror("KVM_GET_CPU_MMAP_SIZE");
        exit(1);
    }

    vcpu->kvm_run = mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE,
            MAP_SHARED, vcpu->fd, 0);
    if (vcpu->kvm_run == MAP_FAILED) {
        perror("mmap kvm_run");
        exit(1);
    }
}

void set_sregs(struct kvm_sregs *sregs) {
    sregs->cs.selector = 0;
	sregs->cs.base = 0;
}

void set_regs(struct vcpu *vcpu) {
    if (ioctl(vcpu->fd, KVM_GET_SREGS, &(vcpu->sregs)) < 0) {
        perror("KVM_GET_SREGS");
        exit(1);
    }

    set_sregs(&vcpu->sregs);

    if (ioctl(vcpu->fd, KVM_SET_SREGS, &(vcpu->sregs)) < 0) {
        perror("KVM_SET_SREGS");
        exit(1);
    }

    vcpu->regs.rflags = 0x0000000000000002ULL;
    vcpu->regs.rip = 0x7c00;

    if (ioctl(vcpu->fd, KVM_SET_REGS, &(vcpu->regs)) < 0) {
        perror("KVM_SET_REGS");
        exit(1);
    }
}

void set_irqchip(struct vm *vm) {
    if (ioctl(vm->fd, KVM_CREATE_IRQCHIP, 0) < 0) {
        perror("KVM_CREATE_IRQCHIP");
        exit(1);
    }
}

void load_guest_binary(void *dst) {
    int biosfd = open(GUEST_PATH, O_RDONLY);
    if (biosfd < 0) {
        perror("open fail");
        exit(1);
    }

    int ret = 0;
    char *tmp = (char*)dst; 
    while(1) {
        ret = read(biosfd, tmp + 0x7c00, 4096 * 128);
        if (ret <= 0) break;

        printf("read size: %d\n", ret);
        tmp += ret;
    }
}

void set_user_memory_region(struct vm *vm, 
        struct kvm_userspace_memory_region *memreg) {
    // vm->mem = mmap(NULL, 0x200000, PROT_READ | PROT_WRITE,
	//        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    size_t guest_memory_size = 0x200000;
    size_t alignment_size = 0x1000;

    int result = posix_memalign(&(vm->mem), 
            alignment_size, 
            guest_memory_size);

    if (result != 0) {
        printf("mmap fail: %d\n", result);
        exit(1);
    }

    memreg->slot = 1;
	memreg->flags = 0;
	memreg->guest_phys_addr = 0;
    memreg->memory_size = 0x200000;
	//memreg->memory_size = (__u64)guest_memory_size;
	memreg->userspace_addr = (unsigned long)vm->mem;
    
    if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, memreg) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
        exit(1);
	}
}

void print_regs(struct vcpu *vcpu) {
    ioctl(vcpu->fd, KVM_GET_REGS, &(vcpu->regs));
    printf("rip: 0x%llx\n", vcpu->regs.rip);
}

void set_blk(struct blk *blk) {
    blk = (struct blk*)malloc(sizeof(struct blk));
    blk->data = malloc(5120000);
    // posix_memalign(blk->data, 
    //         4096, 
    //         5120);


    int blk_img_fd = open("xv6.img", O_RDONLY);
    if (blk_img_fd < 0) {
        perror("fail open xv6.img");
        exit(1);
    }

    int read_size = 0;
    void *tmp = (void*)blk->data;
    while(1) {
        read_size = read(blk_img_fd, tmp, 512000);
        if (read_size <= 0) break;

        printf("read blk: %d\n", read_size);

        tmp += read_size;
    }
    exit(1);
} 

int main(int argc, char **argv) {
    struct vm *vm = (struct vm*)malloc(sizeof(struct vm));
    struct vcpu *vcpu = (struct vcpu*)malloc(sizeof(struct vcpu));
    struct kvm_userspace_memory_region *memreg = (struct kvm_userspace_memory_region*)malloc(sizeof(struct kvm_userspace_memory_region));
    struct blk *blk;
    // blk->data = (void*)malloc(1024 * 1024 * 10);

    vm->vm_fd = open("/dev/kvm", O_RDWR);
    if (vm->vm_fd < 0) { 
        perror("open /dev/kvm/");
        exit(1);
    }

    vm->fd = ioctl(vm->vm_fd, KVM_CREATE_VM, 0);
    if (vm->fd < 0) {
        perror("KVM_CREATE_VM");
        exit(1);
    }

    set_irqchip(vm);

    if (ioctl(vm->fd, KVM_CREATE_PIT) < 0) {
        perror("KVM_CREATE_PIT");
        exit(1);
    }

    if (ioctl(vm->fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
        perror("KVM_SET_TSS_ADDR");
        exit(1);
    }

    set_user_memory_region(vm, memreg);
    
    load_guest_binary(vm->mem);

    init_vcpu(vm, vcpu);
    set_regs(vcpu);

    set_blk(blk);

    for (;;) {
        if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
            perror("KVM_RUN");
            exit(1);
        }

        if (vcpu->kvm_run->exit_reason == KVM_EXIT_IO) {
            if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT) {
                for (int i = 0; i < vcpu->kvm_run->io.count; i++) {
                    int port = vcpu->kvm_run->io.port;
                    
                    printf("out: %u\n", port);
                    print_regs(vcpu);
		        }
            } else {
                printf("in: %u\n", vcpu->kvm_run->io.port);
                print_regs(vcpu);
            }
        } else if (vcpu->kvm_run->exit_reason == KVM_EXIT_EXCEPTION) {
            printf("exception\n");
        } else {
            printf("unknown: %d\n", vcpu->kvm_run->exit_reason);
            exit(1);
        }
    }

    return 1;   
}