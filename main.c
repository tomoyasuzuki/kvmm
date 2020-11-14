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

#define GUEST_PATH "guest.bin"


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

#define CODE_START 0x1000

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

    //memset(&(vcpu->regs), 0, sizeof(vcpu->regs));

    vcpu->regs.rflags = 0x0000000000000002ULL;
    vcpu->regs.rip = 0;
    vcpu->regs.rsp = 0xffffffff;
    vcpu->regs.rbp = 0x000e0000;

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
        ret = read(biosfd, tmp, 4096);
        if (ret <= 0) break;
        printf("reg %d", ret);

        printf("read size: %d\n", ret);
        tmp += ret;
    }
}

void set_user_memory_region(void **mem, int vmfd, 
        struct kvm_userspace_memory_region *memreg) {
    *mem = mmap(NULL, 0x200000, PROT_READ | PROT_WRITE,
	        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    //int mem = posix_memalign(&(mem), (size_t)4096, (size_t)0x200000);
    //printf("posix_memalign: %d\n", mem);
    if (*mem == (void*)(-1)) {
        perror("mmap mem");
        exit(1);
    }

    memreg->slot = 0;
	memreg->flags = 0;
	memreg->guest_phys_addr = 0;
	memreg->memory_size = 0x200000;
	memreg->userspace_addr = (unsigned long)*mem;
    
    if (ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, memreg) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
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

    //set_irqchip(vm);

    if (ioctl(vm->fd, KVM_CREATE_PIT) < 0) {
        perror("KVM_CREATE_PIT");
        exit(1);
    }

    if (ioctl(vm->fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
        perror("KVM_SET_TSS_ADDR");
        exit(1);
    }

    set_user_memory_region(&vm->mem, vm->fd, &memreg);
    
    load_guest_binary(vm->mem);

    init_vcpu(vm, vcpu);
    set_regs(vcpu);

    struct kvm_regs regs;
    uint64_t memval = 0;

    for (;;) {
        if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
            perror("KVM_RUN");
            exit(1);
        }

        switch (vcpu->kvm_run->exit_reason) {
            case KVM_EXIT_HLT:
                printf("HLT\n");
                exit(1);
            case KVM_EXIT_IO:
                goto check;
            default:
                fprintf(stderr, "Got exit_reason %d\n", vcpu->kvm_run->exit_reason);
                exit(1);
        }

        check:
            if (ioctl(vcpu->fd, KVM_GET_REGS, &regs) < 0) {
		        perror("KVM_GET_REGS");
		        exit(1);
	        }

            if (regs.rax != 123) {
                printf("Wrong result: {E,R,}AX is %lld\n", regs.rax);
                return 0;
            }
    }

    return 1;   
}

/*
 * KVM API Sample.
 * author: Xu He Jie xuhj@cn.ibm.com
 */

// #include <stdio.h>
// // #include <stdlib.h>
// #include <unistd.h>
// // #include <fcntl.h>
// // #include <errno.h>
// #include <sys/ioctl.h>
// // #include <sys/mman.h>
// #include <string.h>
// #include <stdint.h>
// // #include <linux/kvm.h>
// #include <errno.h>
// #include <sys/ioctl.h>
// #include <stdio.h>
// #include <memory.h>
// #include <sys/mman.h>
// #include <pthread.h>
// #include <linux/kvm.h>
// #include <fcntl.h>
// #include <stdlib.h>
// #include <assert.h>

// #define KVM_DEVICE "/dev/kvm"
// #define RAM_SIZE 512000000
// #define CODE_START 0x1000
// #define BINARY_FILE "guest.bin"

// struct kvm {
//    int dev_fd;	
//    int vm_fd;
//    __u64 ram_size;
//    __u64 ram_start;
//    int kvm_version;
//    struct kvm_userspace_memory_region mem;

//    struct vcpu *vcpus;
//    int vcpu_number;
// };

// struct vcpu {
//     int vcpu_id;
//     int vcpu_fd;
//     pthread_t vcpu_thread;
//     struct kvm_run *kvm_run;
//     int kvm_run_mmap_size;
//     struct kvm_regs regs;
//     struct kvm_sregs sregs;
//     void *(*vcpu_thread_func)(void *);
// };

// void kvm_reset_vcpu (struct vcpu *vcpu) {
// 	if (ioctl(vcpu->vcpu_fd, KVM_GET_SREGS, &(vcpu->sregs)) < 0) {
// 		perror("can not get sregs\n");
// 		exit(1);
// 	}

// 	vcpu->sregs.cs.selector = CODE_START;
// 	vcpu->sregs.cs.base = CODE_START * 16;
// 	vcpu->sregs.ss.selector = CODE_START;
// 	vcpu->sregs.ss.base = CODE_START * 16;
// 	vcpu->sregs.ds.selector = CODE_START;
// 	vcpu->sregs.ds.base = CODE_START *16;
// 	vcpu->sregs.es.selector = CODE_START;
// 	vcpu->sregs.es.base = CODE_START * 16;
// 	vcpu->sregs.fs.selector = CODE_START;
// 	vcpu->sregs.fs.base = CODE_START * 16;
// 	vcpu->sregs.gs.selector = CODE_START;

// 	if (ioctl(vcpu->vcpu_fd, KVM_SET_SREGS, &vcpu->sregs) < 0) {
// 		perror("can not set sregs");
// 		exit(1);
// 	}

// 	vcpu->regs.rflags = 0x0000000000000002ULL;
// 	vcpu->regs.rip = 0;
// 	vcpu->regs.rsp = 0xffffffff;
// 	vcpu->regs.rbp= 0;

// 	if (ioctl(vcpu->vcpu_fd, KVM_SET_REGS, &(vcpu->regs)) < 0) {
// 		perror("KVM SET REGS\n");
// 		exit(1);
// 	}
// }

// void *kvm_cpu_thread(void *data) {
// 	struct kvm *kvm = (struct kvm *)data;
// 	int ret = 0;
// 	kvm_reset_vcpu(kvm->vcpus);

// 	while (1) {
// 		printf("KVM start run\n");
// 		ret = ioctl(kvm->vcpus->vcpu_fd, KVM_RUN, 0);
	
// 		if (ret < 0) {
// 			fprintf(stderr, "KVM_RUN failed\n");
// 			exit(1);
// 		}

// 		switch (kvm->vcpus->kvm_run->exit_reason) {
// 		case KVM_EXIT_UNKNOWN:
// 			printf("KVM_EXIT_UNKNOWN\n");
// 			break;
// 		case KVM_EXIT_DEBUG:
// 			printf("KVM_EXIT_DEBUG\n");
// 			break;
// 		case KVM_EXIT_IO:
// 			printf("KVM_EXIT_IO\n");
// 			printf("out port: %d, data: %d\n", 
// 				kvm->vcpus->kvm_run->io.port,  
// 				*(int *)((char *)(kvm->vcpus->kvm_run) + kvm->vcpus->kvm_run->io.data_offset)
// 				);
// 			sleep(1);
// 			break;
// 		case KVM_EXIT_MMIO:
// 			printf("KVM_EXIT_MMIO\n");
// 			break;
// 		case KVM_EXIT_INTR:
// 			printf("KVM_EXIT_INTR\n");
// 			break;
// 		case KVM_EXIT_SHUTDOWN:
// 			printf("KVM_EXIT_SHUTDOWN\n");
// 			goto exit_kvm;
// 			break;
//         case KVM_EXIT_HLT:
//             printf("KVM_EXIT_HLT\n");
//             sleep(1);
//             break;
// 		default:
// 			printf("KVM PANIC\n");
// 			goto exit_kvm;
// 		}
// 	}

// exit_kvm:
// 	return 0;
// }

// void load_binary(struct kvm *kvm) {
//     int fd = open(BINARY_FILE, O_RDONLY);

//     if (fd < 0) {
//         fprintf(stderr, "can not open binary file\n");
//         exit(1);
//     }

//     int ret = 0;
//     char *p = (char *)kvm->ram_start;

//     while(1) {
//         ret = read(fd, p, 4096);
//         if (ret <= 0) {
//             break;
//         }
//         printf("read size: %d", ret);
//         p += ret;
//     }
// }

// struct kvm *kvm_init(void) {
//     struct kvm *kvm = malloc(sizeof(struct kvm));
//     kvm->dev_fd = open(KVM_DEVICE, O_RDWR);

//     if (kvm->dev_fd < 0) {
//         perror("open kvm device fault: ");
//         return NULL;
//     }

//     kvm->kvm_version = ioctl(kvm->dev_fd, KVM_GET_API_VERSION, 0);

//     return kvm;
// }

// void kvm_clean(struct kvm *kvm) {
//     assert (kvm != NULL);
//     close(kvm->dev_fd);
//     free(kvm);
// }

// int kvm_create_vm(struct kvm *kvm, int ram_size) {
//     int ret = 0;
//     kvm->vm_fd = ioctl(kvm->dev_fd, KVM_CREATE_VM, 0);

//     if (kvm->vm_fd < 0) {
//         perror("can not create vm");
//         return -1;
//     }

//     kvm->ram_size = ram_size;
//     kvm->ram_start =  (__u64)mmap(NULL, kvm->ram_size, 
//                 PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, 
//                 -1, 0);

//     if ((void *)kvm->ram_start == MAP_FAILED) {
//         perror("can not mmap ram");
//         return -1;
//     }
    
//     kvm->mem.slot = 0;
//     kvm->mem.guest_phys_addr = 0;
//     kvm->mem.memory_size = kvm->ram_size;
//     kvm->mem.userspace_addr = kvm->ram_start;

//     ret = ioctl(kvm->vm_fd, KVM_SET_USER_MEMORY_REGION, &(kvm->mem));

//     if (ret < 0) {
//         perror("can not set user memory region");
//         return ret;
//     }

//     return ret;
// }

// void kvm_clean_vm(struct kvm *kvm) {
//     close(kvm->vm_fd);
//     munmap((void *)kvm->ram_start, kvm->ram_size);
// }

// struct vcpu *kvm_init_vcpu(struct kvm *kvm, int vcpu_id, void *(*fn)(void *)) {
//     struct vcpu *vcpu = malloc(sizeof(struct vcpu));
//     vcpu->vcpu_id = 0;
//     vcpu->vcpu_fd = ioctl(kvm->vm_fd, KVM_CREATE_VCPU, vcpu->vcpu_id);

//     if (vcpu->vcpu_fd < 0) {
//         perror("can not create vcpu");
//         return NULL;
//     }

//     vcpu->kvm_run_mmap_size = ioctl(kvm->dev_fd, KVM_GET_VCPU_MMAP_SIZE, 0);

//     if (vcpu->kvm_run_mmap_size < 0) {
//         perror("can not get vcpu mmsize");
//         return NULL;
//     }

//     printf("%d\n", vcpu->kvm_run_mmap_size);
//     vcpu->kvm_run = mmap(NULL, vcpu->kvm_run_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu->vcpu_fd, 0);

//     if (vcpu->kvm_run == MAP_FAILED) {
//         perror("can not mmap kvm_run");
//         return NULL;
//     }

//     vcpu->vcpu_thread_func = fn;
//     return vcpu;
// }

// void kvm_clean_vcpu(struct vcpu *vcpu) {
//     munmap(vcpu->kvm_run, vcpu->kvm_run_mmap_size);
//     close(vcpu->vcpu_fd);
// }

// void kvm_run_vm(struct kvm *kvm) {
//     int i = 0;

//     for (i = 0; i < kvm->vcpu_number; i++) {
//         if (pthread_create(&(kvm->vcpus->vcpu_thread), (const pthread_attr_t *)NULL, kvm->vcpus[i].vcpu_thread_func, kvm) != 0) {
//             perror("can not create kvm thread");
//             exit(1);
//         }
//     }

//     pthread_join(kvm->vcpus->vcpu_thread, NULL);
// }

// int main(int argc, char **argv) {
//     int ret = 0;
//     struct kvm *kvm = kvm_init();

//     if (kvm == NULL) {
//         fprintf(stderr, "kvm init fauilt\n");
//         return -1;
//     }

//     if (kvm_create_vm(kvm, RAM_SIZE) < 0) {
//         fprintf(stderr, "create vm fault\n");
//         return -1;
//     }

//     load_binary(kvm);

//     // only support one vcpu now
//     kvm->vcpu_number = 1;
//     kvm->vcpus = kvm_init_vcpu(kvm, 0, kvm_cpu_thread);

//     kvm_run_vm(kvm);

//     kvm_clean_vm(kvm);
//     kvm_clean_vcpu(kvm->vcpus);
//     kvm_clean(kvm);
// }
