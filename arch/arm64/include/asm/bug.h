#ifndef _ASMARM_BUG_H
#define _ASMARM_BUG_H

#include <linux/linkage.h>

#ifdef CONFIG_BUG

#define BUG_INSTR_VALUE 0xe7f001f2
#define BUG_INSTR_TYPE ".word "

#define BUG() _BUG(__FILE__, __LINE__, BUG_INSTR_VALUE)
#define _BUG(file, line, value) __BUG(file, line, value)

#ifdef CONFIG_DEBUG_BUGVERBOSE

/*
 * The extra indirection is to ensure that the __FILE__ string comes through
 * OK. Many version of gcc do not support the asm %c parameter which would be
 * preferable to this unpleasantness. We use mergeable string sections to
 * avoid multiple copies of the string appearing in the kernel image.
 */

#define __BUG(__file, __line, __value)				\
do {								\
	asm volatile("1:\t" BUG_INSTR_TYPE #__value "\n"	\
		".pushsection .rodata.str, \"aMS\", 1\n"	\
		"2:\t.asciz " #__file "\n"			\
		".popsection\n"					\
		".pushsection __bug_table,\"a\"\n"		\
		"3:\t.quad 1b, 2b\n"				\
		"\t.word " #__line ", 0\n"			\
		".popsection");					\
	unreachable();						\
} while (0)

#else  /* not CONFIG_DEBUG_BUGVERBOSE */

#define __BUG(__file, __line, __value)				\
do {								\
	asm volatile(BUG_INSTR_TYPE #__value);			\
	unreachable();						\
} while (0)
#endif  /* CONFIG_DEBUG_BUGVERBOSE */

#define HAVE_ARCH_BUG
#endif  /* CONFIG_BUG */

#include <asm-generic/bug.h>

#endif
