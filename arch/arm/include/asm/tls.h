#ifndef __ASMARM_TLS_H
#define __ASMARM_TLS_H

#ifdef __ASSEMBLY__
#include <asm/asm-offsets.h>
	.macro switch_tls_none, prev, next, tp, tpuser, tmp1, tmp2
	.endm

	.macro switch_tls_v6k, prev, next, tp, tpuser, tmp1, tmp2
	ldrd	\tp, \tpuser, [\next, #TI_TP_VALUE]	@ get the next TLS and user r/w register
	mrc	p15, 0, \tmp2, c13, c0, 2	@ get the user r/w register
	mcr	p15, 0, \tp, c13, c0, 3		@ set TLS register
	mcr	p15, 0, \tpuser, c13, c0, 2	@ and the user r/w register
	str	\tmp2, [\prev, #TI_TP_VALUE + 4] @ save it
	.endm

	.macro switch_tls_v6, prev, next, tp, tpuser, tmp1, tmp2
	ldr	\tmp1, =elf_hwcap
	ldr	\tmp1, [\tmp1, #0]
	mov	\tmp2, #0xffff0fff
	ldr	\tp, [\next, #TI_TP_VALUE]	@ get the next TLS register
	tst	\tmp1, #HWCAP_TLS		@ hardware TLS available?
	streq	\tp, [\tmp2, #-15]		@ set TLS value at 0xffff0ff0
	mrcne	p15, 0, \tmp2, c13, c0, 2	@ get the previous user r/w register
	ldrne	\tpuser, [\next, #TI_TP_VALUE + 4]	@ get the next user r/w register
	mcrne	p15, 0, \tp, c13, c0, 3		@ yes, set TLS register
	mcrne	p15, 0, \tpuser, c13, c0, 2	@ set user r/w register
	strne	\tmp2, [\prev, #TI_TP_VALUE + 4] @ save it
	.endm

	.macro switch_tls_software, prev, next, tp, tpuser, tmp1, tmp2
	mov	\tmp1, #0xffff0fff
	str	\tp, [\tmp1, #-15]		@ set TLS value at 0xffff0ff0
	.endm
#endif

#ifdef CONFIG_TLS_REG_EMUL
#define tls_emu		1
#define has_tls_reg		1
#define switch_tls	switch_tls_none
#elif defined(CONFIG_CPU_V6)
#define tls_emu		0
#define has_tls_reg		(elf_hwcap & HWCAP_TLS)
#define switch_tls	switch_tls_v6
#elif defined(CONFIG_CPU_32v6K)
#define tls_emu		0
#define has_tls_reg		1
#define switch_tls	switch_tls_v6k
#else
#define tls_emu		0
#define has_tls_reg		0
#define switch_tls	switch_tls_software
#endif

#ifndef __ASSEMBLY__
static inline unsigned long get_tpuser(void)
{
	unsigned long reg = 0;

	if (has_tls_reg && !tls_emu)
		__asm__("mrc p15, 0, %0, c13, c0, 2" : "=r" (reg));

	return reg;
}
#endif
#endif	/* __ASMARM_TLS_H */
