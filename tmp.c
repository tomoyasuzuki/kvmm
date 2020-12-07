// void *dst = vm->mem + 0x400;
//     // //0x80000000
//     // //0xFFFFFFFF
//     // // 0x400

//     // char *mp_s = "_MP_";

//     struct mp *mp = (struct mp*)malloc(sizeof(struct mp));
//     mp->signature[0] = (unsigned char)"_";
//     mp->signature[1] = "M";
//     mp->signature[2] = "P";
//     mp->signature[3] = "_";
//     mp->imcrp = 0;
//     mp->physaddr = (void*)((uint64_t)vm->mem +  0x400 + (uint64_t)(sizeof(struct mp)));
//     mp->checksum = 0;
//     mp->length = 1024;
//     mp->reserved[0] = 0;
//     mp->specrev = 0;
//     mp->type = 0;
//     struct mpconf *conf = (struct mpconf*)malloc(sizeof(struct mpconf));
//     conf->signature[0] = "P";
//     conf->signature[1] = "C";
//     conf->signature[2] = "M";
//     conf->signature[3] = "P";
//     conf->version = 4;
//     conf->length = sizeof(struct mpconf);
//     conf->lapicaddr = (void*)((uint64_t)mp->physaddr +(uint64_t)sizeof(struct mpconf));;

//     memcpy(dst, mp, sizeof(struct mp));
//     memcpy(mp->physaddr, conf, sizeof(struct mpconf));

//     //     for (int i = 0; i < vcpu->kvm_run->io.count; i++) {

//             //         char value = *(unsigned char *)((unsigned char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset);
//             //         u16 val = *(u16 *)((u16 *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset);
//             //         ioctl(vcpu->fd, KVM_GET_REGS, &(vcpu->regs));

//             //         if (port == 0x3f8) {
//             //             char value[100];
//             //             for (int i_out = 0; i_out < vcpu->kvm_run->io.count; i_out++) {
//             //                 char *v = (char*)((unsigned char*)vcpu->kvm_run + vcpu->kvm_run->io.data_offset);
//             //                 write(outfd, v, 1);
//             //                 vcpu->kvm_run->io.data_offset += vcpu->kvm_run->io.size;
//             //             }
//             //             break;
//             //         }
//             //         printf("out: %d\n", vcpu->kvm_run->io.port);
//             //         print_regs(vcpu);
//             //         handle_io_out(blk, port, value, val);
// 		    //    }