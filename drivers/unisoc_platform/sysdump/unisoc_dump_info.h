/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __UNISOC_DUMP_INFO_H__
#define __UNISOC_DUMP_INFO_H__

#include <linux/seq_buf.h>

/* GKI requires the NR_CPUS is 32 */
#if NR_CPUS >= 8
#define UNISOC_NR_CPUS            8
#else
#define UNISOC_NR_CPUS            NR_CPUS
#endif
#define UNISOC_DUMP_RQ_SIZE	(2000 * UNISOC_NR_CPUS)
#define UNISOC_DUMP_MAX_TASK	3000
#define UNISOC_DUMP_TASK_SIZE	(160 * (UNISOC_DUMP_MAX_TASK + 2))
#define UNISOC_DUMP_STACK_SIZE	(2048 * UNISOC_NR_CPUS)
#define UNISOC_DUMP_MEM_SIZE	4096 /* 4K */
#define UNISOC_DUMP_IRQ_SIZE	12288
#define MAX_CALLBACK_LEVEL	16
#define MAX_NAME_LEN		16

#define SEQ_printf(m, x...)			\
do {						\
	if (m)					\
		seq_buf_printf(m, x);		\
	else					\
		pr_debug(x);			\
} while (0)

#if IS_ENABLED(CONFIG_UNISOC_LASTKMSG)

extern int minidump_add_section(const char *name, int size, struct seq_buf **save_buf);
extern void minidump_release_section(const char *name, struct seq_buf *save_buf);

/*
 * save per-cpu's stack and regs in sysdump.
 *
 * @cpu:	the cpu number;
 *
 * @pregs:	pt_regs;
 */
extern void unisoc_dump_stack_reg(int cpu, struct pt_regs *pregs);
extern void unisoc_dump_task_stats(void);
extern void unisoc_dump_runqueues(void);
extern void unisoc_dump_mem_info(void);

/*
 * update current task's stack's phy addr in sysdump.
 */
extern void minidump_update_current_stack(int cpu, struct pt_regs *regs);

#if IS_ENABLED(CONFIG_UNISOC_DUMP_IO)
extern int sprd_dump_io_init(void);
extern void sprd_dump_io_exit(void);
#else
static int sprd_dump_io_init(void)
{
	return -EINVAL;
}
static void sprd_dump_io_exit(void) {}
#endif
#else
static int minidump_add_section(const char *name, int size, struct seq_buf **save_buf)
{
	return -EINVAL;
}
static void minidump_release_section(const char *name, struct seq_buf *save_buf) {}
static inline void unisoc_dump_stack_reg(int cpu, struct pt_regs *pregs) {}
static inline void unisoc_dump_task_stats(void) {}
static inline void unisoc_dump_runqueues(void) {}
static inline void minidump_update_current_stack(int cpu, struct pt_regs *regs) {}
static inline void unisoc_dump_mem_info(void) {}

#endif

#endif /* __UNISOC_DUMP_INFO_H */
