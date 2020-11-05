/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

/* common and private utility for mtprof */
long long msec_high(unsigned long long nsec);
unsigned long msec_low(unsigned long long nsec);
long long usec_high(unsigned long long nsec);
long long sec_high(unsigned long long nsec);
unsigned long sec_low(unsigned long long nsec);

void mt_irq_monitor_test_init(struct proc_dir_entry *dir);
void irq_count_tracer_init(void);
const char *irq_to_name(int irq);
