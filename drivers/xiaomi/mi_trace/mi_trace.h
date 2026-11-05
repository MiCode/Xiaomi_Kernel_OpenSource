/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __MI_TRACE_H__
#define __MI_TRACE_H__

#include <linux/sched.h>

#define MI_TRACE_IRQ_INFO_SIZE 16
#define MI_TRACE_TASK_INFO_SIZE 40
#define MI_TRACE_HOOK_ON 1

/* irq */
struct mi_trace_irq_info {
	u64 clock;
	u32 irq;
	u8 dir;
};

/* task */
struct mi_trace_task_info {
	u64 clock;
	uintptr_t stack;
	u64 pid;
	char comm[TASK_COMM_LEN];
};

struct mi_trace_buffer_info {
	unsigned char *buffer_addr;
	unsigned char *percpu_addr[NR_CPUS];
	unsigned int percpu_length;
	unsigned int buffer_size;
};

struct mi_trace_pinfo {
	u32 max_num;
	u32 field_len;
	u32 rear;
	u32 is_full;
	u8 data[1];
};

struct mi_trace_mem_node {
	uintptr_t paddr;
	uintptr_t vaddr;
	unsigned int size;
};

enum mi_trace_type {
	TR_IRQ = 0,
	TR_TASK,
	TR_MAX
};

#endif /* __MI_TRACE_H__ */
