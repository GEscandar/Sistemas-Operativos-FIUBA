/*
 * Your code here for context_switch
 *
 * It should restores the register values in the Trapframe with the 'iret' instruction.
 * This exits the kernel and starts executing some environment's code.
 *
 * This function does not return.
 */
// void context_switch(struct Trapframe *tf)
.globl context_switch;
context_switch:
	// Point the stack pointer to the trapframe stored in memory,
	// assuming System V i386 calling convention
	movl 0x4(%esp),%esp 

	// Initialize new process context with what's stored in 
	// the trapframe
	popal			// pop all general purpose registers except esp (eax, ebx, ecx, edx, ebp, esi, edi)
	popl %es		// pop extra segment
	popl %ds		// pop data segment
	addl $0x8,%esp	// move the stack pointer 8 bytes: 4 for tf_trapno and 4 for tf_err (to ignore them)
	iret			// load ss,cs,eflags and eip of the new process

spin:
	jmp spin
