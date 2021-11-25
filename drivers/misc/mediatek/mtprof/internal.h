/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _IRQ_MON_INT_H
#define _IRQ_MON_INT_H

/* common and private utility for mtprof */
long long msec_high(unsigned long long nsec);
unsigned long msec_low(unsigned long long nsec);
long long usec_high(unsigned long long nsec);
long long sec_high(unsigned long long nsec);
unsigned long sec_low(unsigned long long nsec);

void mt_irq_monitor_test_init(struct proc_dir_entry *dir);

// irq count tracer
int irq_count_tracer_init(void);
void irq_count_tracer_exit(void);
const char *irq_to_name(int irq);
const void *irq_to_handler(int irq);
const int irq_to_ipi_type(int irq);
void show_irq_count_info(unsigned int output);
void irq_count_tracer_set(bool val);
void irq_count_tracer_proc_init(struct proc_dir_entry *parent);
#define TO_FTRACE     (1U << 0)
#define TO_KERNEL_LOG (1U << 1)
#define TO_AEE        (1U << 2)
#define TO_SRAM       (1U << 3)
#define TO_BOTH       (TO_FTRACE | TO_KERNEL_LOG)

#define MAX_MSG_LEN 160

void irq_mon_msg(unsigned int out, char *buf, ...);

// proc
int irq_mon_bool_open(struct inode *inode, struct file *file);
ssize_t irq_mon_count_set(struct file *filp,
		const char *ubuf, size_t count, loff_t *data);
bool irq_mon_aee_debounce_check(bool update);

extern const struct proc_ops irq_mon_uint_pops;

#define IRQ_MON_TRACER_PROC_ENTRY(name, mode, type, dir, ptr) \
	proc_create_data(#name, mode, dir, &irq_mon_##type##_pops, (void *)ptr)
#endif

