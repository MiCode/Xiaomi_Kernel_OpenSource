/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/io.h>
#include <linux/notifier.h>

#include "mtk_qos_prefetch.h"
#include "mtk_qos_ipi.h"

static int qos_prefetch_enabled;
static int qos_prefetch_log_enabled;
static unsigned int qos_prefetch_count;
static unsigned int qos_prefetch_buf[3];
static BLOCKING_NOTIFIER_HEAD(qos_prefetch_chain_head);

int is_qos_prefetch_enabled(void)
{
	return qos_prefetch_enabled;
}
EXPORT_SYMBOL(is_qos_prefetch_enabled);

void qos_prefetch_enable(int enable)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_PREFETCH_ENABLE;
	qos_ipi_d.u.qos_prefetch_enable.enable = enable;
	qos_ipi_to_sspm_command(&qos_ipi_d, 2);
#endif
	qos_prefetch_enabled = enable;
}
EXPORT_SYMBOL(qos_prefetch_enable);

int is_qos_prefetch_log_enabled(void)
{
	return qos_prefetch_log_enabled;
}
EXPORT_SYMBOL(is_qos_prefetch_log_enabled);

void qos_prefetch_log_enable(int enable)
{
	qos_prefetch_log_enabled = enable;
}
EXPORT_SYMBOL(qos_prefetch_log_enable);

unsigned int get_qos_prefetch_count(void)
{
	return qos_prefetch_count;
}
EXPORT_SYMBOL(get_qos_prefetch_count);

unsigned int *get_qos_prefetch_buf(void)
{
	return qos_prefetch_buf;
}
EXPORT_SYMBOL(get_qos_prefetch_buf);

int register_prefetch_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register
		(&qos_prefetch_chain_head, nb);
}
EXPORT_SYMBOL(register_prefetch_notifier);

int unregister_prefetch_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister
		(&qos_prefetch_chain_head, nb);
}
EXPORT_SYMBOL(unregister_prefetch_notifier);

int prefetch_notifier_call_chain(unsigned long val, void *v)
{
	int ret = NOTIFY_DONE;
	int i;

	if (!is_qos_prefetch_enabled())
		return ret;

	if (val > 0 && val <= 4) {
		for (i = 0; (val & (1 << i)) == 0; i++)
			;
		qos_prefetch_count++;
		qos_prefetch_buf[i]++;
	}

	if (is_qos_prefetch_log_enabled()) {
		pr_info("#@# %s(%d) val 0x%lx\n",
				__func__, __LINE__, val);
	}

	ret = blocking_notifier_call_chain(&qos_prefetch_chain_head, val, v);

	return notifier_to_errno(ret);
}
EXPORT_SYMBOL(prefetch_notifier_call_chain);

void qos_prefetch_init(void)
{
	qos_prefetch_enable(1);
}
EXPORT_SYMBOL(qos_prefetch_init);

