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
#define GUEST_PATH "bootblock"

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

void set_sregs(struct kvm_sregs *sregs) {
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

    //memset(&(vcpu->regs), 0, sizeof(vcpu->regs));

    vcpu->regs.rflags = 0x0000000000000002ULL;
    vcpu->regs.rip = 0;
    vcpu->regs.rsp = 0xffffffff;
    vcpu->regs.rbp = 0;

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
        ret = read(biosfd, tmp, 4096 * 128);
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
    printf("rip: 0x%llx\n", vcpu->regs.rip);
    printf("rsp: 0x%llx\n", vcpu->regs.rsp);
    printf("rbp: 0x%llx\n", vcpu->regs.rbp);
}

void serial_handle_io(struct kvm_run *run)
{
	unsigned int i;

	switch (run->io.direction) {
	case KVM_EXIT_IO_IN:
		for (i = 0; i < run->io.count; i++) {
			switch (run->io.size) {
			case 1:
				*(unsigned char *)((unsigned char *)run + run->io.data_offset) = 0;
				break;
			case 2:
				*(unsigned short *)((unsigned char *)run + run->io.data_offset) = 0;
				break;
			case 4:
				*(unsigned short *)((unsigned char *)run + run->io.data_offset) = 0;
				break;
			}
			run->io.data_offset += run->io.size;
		}
        //break;

	case KVM_EXIT_IO_OUT:
		for (i = 0; i < run->io.count; i++) {
			putchar(*(char *)((unsigned char *)run + run->io.data_offset));
			run->io.data_offset += run->io.size;
		}
        //break;
	}
}

int main(int argc, char **argv) {
    struct vm *vm = (struct vm*)malloc(sizeof(struct vm));
    struct vcpu *vcpu = (struct vcpu*)malloc(sizeof(struct vcpu));
    struct kvm_userspace_memory_region *memreg = (struct kvm_userspace_memory_region*)malloc(sizeof(struct kvm_userspace_memory_region));

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

    struct kvm_regs regs;

    for (;;) {
        if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
            perror("KVM_RUN");
            exit(1);
        }

        // switch (vcpu->kvm_run->exit_reason) {
        //     case KVM_EXIT_HLT:
        //         printf("HLT\n");
        //         exit(1);
        //     case KVM_EXIT_IO:
        //         if (vcpu->kvm_run->io.port == 0x042 && vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT) {
        //             serial_handle_io(vcpu->kvm_run);
                    
        //         }
        //     default:
        //         printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
        //         //print_regs(vcpu);
        //         //exit(1);
        // }

        if (vcpu->kvm_run->exit_reason == KVM_EXIT_IO) {
            if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT) {
                for (int i = 0; i < vcpu->kvm_run->io.count; i++) {
                    int port = vcpu->kvm_run->io.port;
                    
                    printf("io out: %u\n", port);
                    
                    putchar(*(char *)((unsigned char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset));
			        vcpu->kvm_run->io.data_offset += vcpu->kvm_run->io.size;
		        }
            } else {
                printf("io_in: %u\n", vcpu->kvm_run->io.port);
                for (int i = 0; i < vcpu->kvm_run->io.count; i++) {
			        switch (vcpu->kvm_run->io.size) {
                    case 1:
                        *(unsigned char *)((unsigned char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset) = 0;
                        break;
                    case 2:
                        *(unsigned short *)((unsigned char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset) = 0;
                        break;
                    case 4:
                        *(unsigned short *)((unsigned char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset) = 0;
                        break;
                    }
			        vcpu->kvm_run->io.data_offset += vcpu->kvm_run->io.size;
		        }
            }
        } else if (vcpu->kvm_run->exit_reason == KVM_EXIT_EXCEPTION) {
            printf("Exception\n");
        }
    }

    return 1;   
}