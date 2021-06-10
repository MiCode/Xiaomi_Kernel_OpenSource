// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/scmi_protocol.h>
#include <linux/module.h>
#include <tinysys-scmi.h>

#include "mtk_cm_ipi.h"

//static phys_addr_t mem_phys_addr, mem_virt_addr;
//static unsigned long long mem_size;
static int cm_sspm_ready;
static int cm_ipi_enable = 1;
static int scmi_cm_id;
static struct scmi_tinysys_info_st *_tinfo;




int cm_get_ipi_enable(void)
{
	return cm_ipi_enable;
}
EXPORT_SYMBOL(cm_get_ipi_enable);

void cm_set_ipi_enable(int enable)
{
	cm_ipi_enable = enable;
}
EXPORT_SYMBOL(cm_set_ipi_enable);

void cm_sspm_enable(enable)
{
	cm_mgr_to_sspm_command(IPI_CM_MGR_ENABLE, enable);
}
EXPORT_SYMBOL(cm_sspm_enable);


unsigned int cm_mgr_to_sspm_command(unsigned int cmd, unsigned int val)
{
	struct cm_ipi_data cm_ipi_d;
	unsigned int ret;

	cm_ipi_d.cmd = cmd;
	cm_ipi_d.arg = val;


	if (cm_sspm_ready != 1) {
		pr_info("cm ipi not ready, skip cmd=%d\n", cm_ipi_d.cmd);
		goto error;
	}

	pr_info("#@# %s(%d) cmd 0x%x, arg 0x%x\n", __func__, __LINE__, cm_ipi_d.cmd, cm_ipi_d.arg);
	ret = scmi_tinysys_common_set(_tinfo->ph, scmi_cm_id,
			cm_ipi_d.cmd, cm_ipi_d.arg, 0, 0, 0);
	if (ret) {
		pr_info("cm ipi cmd %d send fail, ret = %d\n",
				cm_ipi_d.cmd, ret);
		goto error;
	}

	return ret;
error:
	return -1;
}
EXPORT_SYMBOL(cm_mgr_to_sspm_command);

void cm_ipi_init(void)
{
	unsigned int ret;

	_tinfo = get_scmi_tinysys_info();

	ret = of_property_read_u32(_tinfo->sdev->dev.of_node, "scmi_cm",
			&scmi_cm_id);
	if (ret) {
		pr_info("get scmi_cm fail, ret %d\n", ret);
		cm_sspm_ready = -2;
		return;
	}
	pr_info("#@# %s(%d) scmi_cm_id %d\n", __func__, __LINE__, scmi_cm_id);

	cm_sspm_ready = 1;
	cm_sspm_enable(cm_ipi_enable);
	pr_info("cm ipi is ready!\n");
}
EXPORT_SYMBOL(cm_ipi_init);

MODULE_DESCRIPTION("CM ipi Driver v0.1");
MODULE_LICENSE("GPL");
