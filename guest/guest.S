	.code16gcc
	.text
	.globl _start
	.type _start, @function

_start:
	mov $0x0000, %ax
	cpuid
	sub $0x2020, %ax 
	sub $0x2020, %bx 
	sub $0x2020, %cx 

	hlt
