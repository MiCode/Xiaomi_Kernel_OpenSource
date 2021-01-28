// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/*
 * These are dummy functions for the case that any aee config is disabled
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <mt-plat/aee.h>
#include "aee-common.h"

struct proc_dir_entry;

#ifndef CONFIG_MTK_AEE_FEATURE

__weak void aee_sram_printk(const char *fmt, ...)
{
}
EXPORT_SYMBOL(aee_sram_printk);

__weak void aee_wdt_fiq_info(void *arg, void *regs, void *svc_sp)
{
}

__weak struct aee_oops *aee_oops_create(enum AE_DEFECT_ATTR attr,
				enum AE_EXP_CLASS clazz, const char *module)
{
	return NULL;
}

__weak void aee_oops_free(struct aee_oops *oops)
{
}

__weak void aee_kernel_exception_api(const char *file, const int line,
				const int db_opt,
				const char *module, const char *msg, ...)
{
}
EXPORT_SYMBOL(aee_kernel_exception_api);

__weak void aee_kernel_warning_api(const char *file, const int line,
				const int db_opt,
				const char *module, const char *msg, ...)
{
}
EXPORT_SYMBOL(aee_kernel_warning_api);

__weak void aee_kernel_reminding_api(const char *file, const int line,
				const int db_opt,
				const char *module, const char *msg, ...)
{
}
EXPORT_SYMBOL(aee_kernel_reminding_api);

__weak void aed_md_exception_api(const int *log, int log_size,
				const int *phy, int phy_size,
				const char *detail, const int db_opt)
{
}
EXPORT_SYMBOL(aed_md_exception_api);

__weak void aed_md32_exception_api(const int *log, int log_size,
				const int *phy, int phy_size,
				const char *detail, const int db_opt)
{
}
EXPORT_SYMBOL(aed_md32_exception_api);

__weak void aed_scp_exception_api(const int *log, int log_size,
				const int *phy, int phy_size,
				const char *detail, const int db_opt)
{
}
EXPORT_SYMBOL(aed_scp_exception_api);

__weak void aed_combo_exception_api(const int *log, int log_size,
				const int *phy, int phy_size,
				const char *detail, const int db_opt)
{
}
EXPORT_SYMBOL(aed_combo_exception_api);

__weak void aed_common_exception_api(const char *assert_type, const int *log,
				int log_size, const int *phy, int phy_size,
				const char *detail, const int db_opt)
{
}
EXPORT_SYMBOL(aed_common_exception_api);

__weak void mt_fiq_printf(const char *fmt, ...)
{
}

__weak void aee_register_api(struct aee_kernel_api *aee_api)
{
}

__weak int aee_nested_printf(const char *fmt, ...)
{
	return 0;
}

__weak int aee_in_nested_panic(void)
{
	return 0;
}

__weak void aee_wdt_printf(const char *fmt, ...)
{
}

__weak int aed_proc_debug_init(struct proc_dir_entry *aed_proc_dir)
{
	return 0;
}

__weak int aed_proc_debug_done(struct proc_dir_entry *aed_proc_dir)
{
	return 0;
}

__weak void aee_rr_proc_init(struct proc_dir_entry *aed_proc_dir)
{
}

__weak void aee_rr_proc_done(struct proc_dir_entry *aed_proc_dir)
{
}

__weak void aee_disable_api(void)
{
}

#endif

__weak int mtk_rgu_status_is_sysrst(void)
{
	return 0;
}

__weak int mtk_rgu_status_is_eintrst(void)
{
	return 0;
}

__weak void show_task_mem(void)
{
}
