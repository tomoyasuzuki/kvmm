.text
.globl _start
.code16
_start:
    xorw %ax, %ax
loop:
    hlt
    jmp loop