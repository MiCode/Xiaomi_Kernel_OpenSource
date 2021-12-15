/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/


#ifndef __MTK_UART_INTERNAL__
#define __MTK_UART_INTERNAL__

/* define sysfs entry for configuring debug level and sysrq */
ssize_t mtk_uart_attr_show(struct kobject *kobj,
	struct attribute *attr, char *buffer);
ssize_t mtk_uart_attr_store(struct kobject *kobj,
	struct attribute *attr, const char *buffer, size_t size);
ssize_t mtk_uart_debug_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_debug_store(struct kobject *kobj,
	const char *page, size_t size);
ssize_t mtk_uart_sysrq_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_sysrq_store(struct kobject *kobj,
	const char *page, size_t size);
ssize_t mtk_uart_vffsz_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_vffsz_store(struct kobject *kobj,
	const char *page, size_t size);
ssize_t mtk_uart_conse_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_conse_store(struct kobject *kobj,
	const char *page, size_t size);
ssize_t mtk_uart_vff_en_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_vff_en_store(struct kobject *kobj,
	const char *page, size_t size);
ssize_t mtk_uart_lsr_status_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_lsr_status_store(struct kobject *kobj,
	const char *page, size_t size);
ssize_t mtk_uart_history_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_history_store(struct kobject *kobj,
	const char *page, size_t size);
/*
 *#if defined(CONFIG_MTK_HDMI_SUPPORT)
 *#include "hdmi_cust.h"
 *extern bool is_hdmi_active(void);
 *extern void hdmi_force_on(int from_uart_drv);
 *#endif
 */
#ifndef CONFIG_FIQ_DEBUGGER
#ifdef CONFIG_MTK_PRINTK_UART_CONSOLE
extern int printk_disable_uart;
extern int mt_need_uart_console;
#endif
#endif

#endif				/* #ifndef __MTK_UART_INTERNAL__ */
