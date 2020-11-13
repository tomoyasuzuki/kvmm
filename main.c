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

/* CR0 bits */
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

/* 64-bit page * entry bits */
#define PDE64_PRESENT 1
#define PDE64_RW (1U << 1)
#define PDE64_USER (1U << 2)
#define PDE64_ACCESSED (1U << 5)
#define PDE64_DIRTY (1U << 6)
#define PDE64_PS (1U << 7)
#define PDE64_G (1U << 8)

#define EFER_SCE 1
#define EFER_LME (1U << 8)
#define EFER_LMA (1U << 10)
#define EFER_NXE (1U << 11)

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

#define CODE_START 0x0

void set_sregs(struct kvm_sregs *sregs) {
    // struct kvm_segment seg = {
    //     .base = 0,
    //     .limit = 0xffffffff,
    //     .selector = 1 << 3,
    //     .present = 1,
    //     .type = 10,
    //     .dpl = 0,
    //     .db = 1,
    //     .s = 1,
    //     .l = 0,
    //     .g = 1
    // };

    // sregs->cr0 |= CR0_PE;
    // sregs->cs = seg;
    // seg.type = 3;
    // seg.selector = 2 << 3;
    // sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = seg;

    sregs->cs.selector = CODE_START;
	sregs->cs.base = CODE_START * 16;
	sregs->ss.selector = CODE_START;
	sregs->ss.base = CODE_START * 16;
	sregs->ds.selector = CODE_START;
	sregs->ds.base = CODE_START *16;
	sregs->es.selector = CODE_START;
	sregs->es.base = CODE_START * 16;
	sregs->fs.selector = CODE_START;
	sregs->fs.base = CODE_START * 16;
	sregs->gs.selector = CODE_START;

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

    memset(&(vcpu->regs), 0, sizeof(vcpu->regs));

    vcpu->regs.rflags = 2;
    vcpu->regs.rip = 0;
    vcpu->regs.rsp = 0xfffffff;
    vcpu->regs.rbp = 0;

    if (ioctl(vcpu->fd, KVM_SET_REGS, &(vcpu->regs)) < 0) {
        perror("KVM_SET_REGS");
        exit(1);
    }
}

int main(int argc, char **argv) {
    struct vm *vm = (struct vm*)malloc(sizeof(struct vm));
    struct vcpu *vcpu = (struct vcpu*)malloc(sizeof(struct vcpu));
    struct kvm_userspace_memory_region memreg;

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

    if (ioctl(vm->fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
        perror("KVM_SET_TSS_ADDR");
        exit(1);
    }

    vm->mem = mmap(NULL, 0x200000, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (vm->mem == (void*)(-1)) {
        perror("mmap mem");
        exit(1);
    }

    memreg.slot = 0;
	memreg.flags = 0;
	memreg.guest_phys_addr = 0;
	memreg.memory_size = 0x200000;
	memreg.userspace_addr = (unsigned long)vm->mem;
    
    if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &memreg) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
        exit(1);
	}

    int guestfd = open("guest.bin", O_RDONLY);

    if (guestfd < 0) {
        perror("open guest binary");
        exit(1);
    }

    int ret = 0;
    char *tmp = (char*)vm->mem; 
    while(1) {
        ret = read(vm->fd, tmp, 4096);
        if (ret <= 0) break;

        printf("read size: %d\n", ret);
        tmp += ret;
    }

    init_vcpu(vm, vcpu);
    set_regs(vcpu);

    for (;;) {
        if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
            perror("KVM_RUN");
            exit(1);
        }

        switch (vcpu->kvm_run->exit_reason) {
            case KVM_EXIT_HLT:
                printf("HLT\n");
                exit(1);
            default:
                fprintf(stderr, "Got exit_reason %d\n", vcpu->kvm_run->exit_reason);
                exit(1);
        }
    }

    return 1;   
}