/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

void mtk_ccci_spd_qos_method_init(void);

void set_ccmni_rps(unsigned long value);

void mtk_ccci_spd_qos_set_task(
	struct task_struct *rx_push_task,
	struct task_struct *alloc_bat_task,
	unsigned int irq_id);

int mtk_ccci_get_tx_done_aff(int txq);
