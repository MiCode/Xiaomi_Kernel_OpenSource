/*
 * arch/arm64/lib/call_with_stack.S
 *
 */

#include <linux/linkage.h>
#include <asm/assembler.h>

/*
 * void call_with_stack(void (*fn)(void *), void *arg, void *sp)
 *
 * Change the stack to that pointed at by sp, then invoke fn(arg) with
 * the new stack.
 */
ENTRY(call_with_stack)
        mov     x3, sp
	str	x3, [x2, #-8]!
	str	lr, [x2, #-8]!

	mov	sp, x2
	mov	x2, x0
	mov	x0, x1

	adr	lr, 1f
	br	x2

1:	ldr	lr, [sp]
	ldr	x3, [sp, #8]
        mov     sp, x3
	br	lr
ENDPROC(call_with_stack)
