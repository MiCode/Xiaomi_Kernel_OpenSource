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
void irq_count_tracer_init(void);
const char *irq_to_name(int irq);
void show_irq_count_info(int output);

#define TO_FTRACE     1
#define TO_KERNEL_LOG 2
#define TO_BOTH       (TO_FTRACE | TO_KERNEL_LOG)
#define TO_SRAM       4

void irq_mon_msg(int out, char *buf, ...);

#endif

