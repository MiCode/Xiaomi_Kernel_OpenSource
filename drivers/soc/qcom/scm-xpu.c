/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <soc/qcom/scm.h>

#if defined(CONFIG_QCOM_XPU_ERR_FATAL)
#define ERR_FATAL_VAL 0x0
#elif defined(CONFIG_QCOM_XPU_ERR_NONFATAL)
#define ERR_FATAL_VAL 0x1
#endif

#define XPU_ERR_FATAL 0xe

static int __init xpu_err_fatal_init(void)
{
	int ret, response;
	struct {
		unsigned int config;
		unsigned int spare;
	} cmd;
	struct scm_desc desc = {0};

	desc.arginfo = SCM_ARGS(2);
	desc.args[0] = cmd.config = ERR_FATAL_VAL;
	desc.args[1] = cmd.spare = 0;

	if (!is_scm_armv8())
		ret = scm_call(SCM_SVC_MP, XPU_ERR_FATAL, &cmd, sizeof(cmd),
				&response, sizeof(response));
	else
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP, XPU_ERR_FATAL), &desc);

	if (ret != 0)
		pr_warn("Failed to set XPU violations as fatal errors: %d\n",
			ret);
	else
		pr_info("Configuring XPU violations to be fatal errors\n");

	return ret;
}
early_initcall(xpu_err_fatal_init);
