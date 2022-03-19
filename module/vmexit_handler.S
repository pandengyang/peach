	.code64
	.globl _vmexit_handler
	.type _vmexit_handler, @function

_vmexit_handler:
	pushq %r15
	pushq %r14
	pushq %r13
	pushq %r12
	pushq %r11
	pushq %r10
	pushq %r9
	pushq %r8        
	pushq %rdi
	pushq %rsi
	pushq %rbp
	pushq %rbx
	pushq %rdx
	pushq %rcx
	pushq %rax        

	movq %rsp, %rdi
	callq handle_vmexit

	popq %rax
	popq %rcx
	popq %rdx
	popq %rbx
	popq %rbp
	popq %rsi
	popq %rdi 
	popq %r8
	popq %r9
	popq %r10
	popq %r11
	popq %r12
	popq %r13
	popq %r14
	popq %r15

	vmresume

	ret
