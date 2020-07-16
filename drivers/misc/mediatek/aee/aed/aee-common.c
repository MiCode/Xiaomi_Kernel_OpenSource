// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include "aed.h"

static struct aee_kernel_api *g_aee_api;
#define KERNEL_REPORT_LENGTH 344

void aee_register_api(struct aee_kernel_api *aee_api)
{
	g_aee_api = aee_api;
}
EXPORT_SYMBOL(aee_register_api);

void aee_disable_api(void)
{
	if (g_aee_api) {
		pr_info("disable aee kernel api");
		g_aee_api = NULL;
	}
}
EXPORT_SYMBOL(aee_disable_api);

void aee_kernel_exception_api_func(const char *file, const int line,
		const int db_opt, const char *module, const char *msg, ...)
{
	char msgbuf[KERNEL_REPORT_LENGTH];
	int offset = 0;
	va_list args;

	va_start(args, msg);
	offset += snprintf(msgbuf, KERNEL_REPORT_LENGTH, "<%s:%d> ", file,
				line);
	offset += vsnprintf(msgbuf + offset, KERNEL_REPORT_LENGTH - offset, msg,
				args);
	if (g_aee_api && g_aee_api->kernel_reportAPI)
		g_aee_api->kernel_reportAPI(AE_DEFECT_EXCEPTION, db_opt, module,
				msgbuf);
	else
		pr_notice("AEE kernel exception: %s", msgbuf);
	va_end(args);
}
EXPORT_SYMBOL(aee_kernel_exception_api_func);

void aee_kernel_warning_api_func(const char *file, const int line,
		const int db_opt, const char *module, const char *msg, ...)
{
	char msgbuf[KERNEL_REPORT_LENGTH];
	int offset = 0;
	va_list args;

	va_start(args, msg);
	offset += snprintf(msgbuf, KERNEL_REPORT_LENGTH, "<%s:%d> ", file,
			line);
	offset += vsnprintf(msgbuf + offset, KERNEL_REPORT_LENGTH - offset,
			msg, args);

	if (g_aee_api && g_aee_api->kernel_reportAPI) {
		if (module && strstr(module,
			"maybe have other hang_detect KE DB"))
			g_aee_api->kernel_reportAPI(AE_DEFECT_FATAL, db_opt,
				module, msgbuf);
		else
			g_aee_api->kernel_reportAPI(AE_DEFECT_WARNING, db_opt,
				module, msgbuf);
	} else {
		pr_notice("AEE kernel warning: %s", msgbuf);
	}
	va_end(args);
}
EXPORT_SYMBOL(aee_kernel_warning_api_func);

int aee_is_printk_too_much(const char *module)
{
	if (strstr(module, "intk too much"))
		return 1;
	return 0;
}
EXPORT_SYMBOL(aee_is_printk_too_much);


void aee_kernel_reminding_api(const char *file, const int line,
		const int db_opt, const char *module, const char *msg, ...)
{
	char msgbuf[KERNEL_REPORT_LENGTH];
	int offset = 0;
	va_list args;

	va_start(args, msg);
	offset += snprintf(msgbuf, KERNEL_REPORT_LENGTH, "<%s:%d> ",
				file, line);
	offset += vsnprintf(msgbuf + offset, KERNEL_REPORT_LENGTH - offset,
				msg, args);
	if (g_aee_api && g_aee_api->kernel_reportAPI)
		g_aee_api->kernel_reportAPI(AE_DEFECT_REMINDING, db_opt,
				module, msgbuf);
	else
		pr_notice("AEE kernel reminding: %s", msgbuf);
	va_end(args);
}
EXPORT_SYMBOL(aee_kernel_reminding_api);

void aed_md_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt)
{
	pr_debug("%s\n", __func__);
	if (g_aee_api) {
		if (g_aee_api->md_exception) {
			g_aee_api->md_exception("modem", log, log_size, phy,
					phy_size, detail, db_opt);
		} else {
			pr_debug("g_aee_api->md_exception = 0x%p\n",
					g_aee_api->md_exception);
		}
	} else {
		pr_debug("g_aee_api is null\n");
	}
	pr_debug("%s out\n", __func__);
}
EXPORT_SYMBOL(aed_md_exception_api);

void aed_md32_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt)
{
	pr_debug("%s\n", __func__);
	if (g_aee_api) {
		if (g_aee_api->md_exception) {
			g_aee_api->md_exception("md32", log, log_size, phy,
					phy_size, detail, db_opt);
		} else {
			pr_debug("g_aee_api->md32_exception = 0x%p\n",
					g_aee_api->md32_exception);
		}
	} else {
		pr_debug("g_aee_api is null\n");
	}
	pr_debug("%s out\n", __func__);
}
EXPORT_SYMBOL(aed_md32_exception_api);

void aed_scp_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt)
{
	pr_debug("%s\n", __func__);
	if (g_aee_api) {
		if (g_aee_api->md_exception) {
			g_aee_api->md_exception("scp", log, log_size, phy,
					phy_size, detail, db_opt);
		} else {
			pr_debug("g_aee_api->scp_exception = 0x%p\n",
					g_aee_api->scp_exception);
		}
	} else {
		pr_debug("g_aee_api is null\n");
	}
	pr_debug("%s out\n", __func__);
}
EXPORT_SYMBOL(aed_scp_exception_api);


void aed_combo_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt)
{
	pr_debug("aed_combo_exception\n");
	if (g_aee_api) {
		if (g_aee_api->combo_exception) {
			g_aee_api->combo_exception("combo", log, log_size, phy,
					phy_size, detail, db_opt);
		} else {
			pr_debug("g_aee_api->combo_exception = 0x%p\n",
					g_aee_api->combo_exception);
		}
	} else {
		pr_debug("g_aee_api is null\n");
	}
	pr_debug("aed_combo_exception out\n");
}
EXPORT_SYMBOL(aed_combo_exception_api);

void aed_common_exception_api(const char *assert_type, const int *log,
			int log_size, const int *phy, int phy_size,
			const char *detail, const int db_opt)
{
	pr_debug("%s\n", __func__);
	if (g_aee_api) {
		if (g_aee_api->common_exception) {
			g_aee_api->common_exception(assert_type, log, log_size,
					phy, phy_size, detail, db_opt);
		} else {
			pr_debug("g_aee_api->common_exception = 0x%p\n",
					g_aee_api->common_exception);
		}
	} else {
		pr_debug("g_aee_api is null\n");
	}
	pr_debug("%s out\n", __func__);
}
EXPORT_SYMBOL(aed_common_exception_api);
