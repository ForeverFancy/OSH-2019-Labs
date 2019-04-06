.section .init
.global _start

_start:

ldr r0, =0x3F200000

mov r1,#1
lsl r1,#27
str r1,[r0,#8]

mov r1,#1
lsl r1,#29
str r1,[r0,#28]

ldr r2,=0x3F003000
ldr r8,=0x000f4240

loop:
	str r1,[r0,#28]
    ldr r3, [r2, #4]
	delay:
		ldr r7, [r2, #4]
		sub r7,r7,r3
		cmp r7,r8
		blt delay
	str r1,[r0,#40]
    ldr r3, [r2, #4]
    delay2:
        ldr r7, [r2, #4]
        sub r7,r7,r3
        cmp r7,r8
        blt delay2
b loop
