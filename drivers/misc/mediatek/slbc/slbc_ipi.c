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

#include "slbc_ipi.h"
#include "slbc_ops.h"
#include "slbc.h"
#include <mtk_slbc_sram.h>

static int slbc_sspm_ready;
static int slbc_scmi_enable;
static int scmi_slbc_id;
static struct scmi_tinysys_info_st *_tinfo;
static struct slbc_ipi_ops *ipi_ops;
static unsigned int scmi_id;

int slbc_get_scmi_enable(void)
{
	return slbc_scmi_enable;
}
EXPORT_SYMBOL_GPL(slbc_get_scmi_enable);

void slbc_set_scmi_enable(int enable)
{
	slbc_scmi_enable = enable;
}
EXPORT_SYMBOL_GPL(slbc_set_scmi_enable);

int slbc_sspm_slb_disable(int disable)
{
	struct slbc_ipi_data slbc_ipi_d;

	pr_info("#@# %s(%d) disable %d\n", __func__, __LINE__, disable);

	slbc_ipi_d.cmd = IPI_SLB_DISABLE;
	slbc_ipi_d.arg = disable;
	return slbc_scmi_set(&slbc_ipi_d, 2);
}
EXPORT_SYMBOL_GPL(slbc_sspm_slb_disable);

int slbc_sspm_slc_disable(int disable)
{
	struct slbc_ipi_data slbc_ipi_d;

	pr_info("#@# %s(%d) disable %d\n", __func__, __LINE__, disable);

	slbc_ipi_d.cmd = IPI_SLC_DISABLE;
	slbc_ipi_d.arg = disable;
	return slbc_scmi_set(&slbc_ipi_d, 2);
}
EXPORT_SYMBOL_GPL(slbc_sspm_slc_disable);

int slbc_sspm_enable(enable)
{
	struct slbc_ipi_data slbc_ipi_d;

	pr_info("#@# %s(%d) enable %d\n", __func__, __LINE__, enable);
	slbc_set_scmi_enable(enable);

	slbc_ipi_d.cmd = IPI_SLBC_ENABLE;
	slbc_ipi_d.arg = enable;
	return slbc_scmi_set(&slbc_ipi_d, 2);
}
EXPORT_SYMBOL_GPL(slbc_sspm_enable);

int slbc_force_scmi_cmd(unsigned int force)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_FORCE;
	slbc_ipi_d.arg = force;

	return slbc_scmi_set(&slbc_ipi_d, 2);
}
EXPORT_SYMBOL_GPL(slbc_force_scmi_cmd);

int slbc_mic_num_cmd(unsigned int num)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_MIC_NUM;
	slbc_ipi_d.arg = num;

	return slbc_scmi_set(&slbc_ipi_d, 2);
}
EXPORT_SYMBOL_GPL(slbc_mic_num_cmd);

int slbc_inner_cmd(unsigned int inner)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_INNER;
	slbc_ipi_d.arg = inner;

	return slbc_scmi_set(&slbc_ipi_d, 2);
}
EXPORT_SYMBOL_GPL(slbc_inner_cmd);

int slbc_outer_cmd(unsigned int outer)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_OUTER;
	slbc_ipi_d.arg = outer;

	return slbc_scmi_set(&slbc_ipi_d, 2);
}
EXPORT_SYMBOL_GPL(slbc_outer_cmd);

int slbc_suspend_resume_notify(int suspend)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_SUSPEND_RESUME_NOTIFY;
	slbc_ipi_d.arg = suspend;
	return slbc_scmi_set(&slbc_ipi_d, 2);
}
EXPORT_SYMBOL_GPL(slbc_suspend_resume_notify);

int _slbc_request_cache_scmi(void *ptr)
{
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	slbc_ipi_d.cmd = SLBC_IPI(IPI_SLBC_CACHE_REQUEST_FROM_AP, d->uid);
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_CACHE) {
		ret = slbc_scmi_get(&slbc_ipi_d, 1, &rvalue);
		if (!ret) {
			d->paddr = (void __iomem *)(long long)rvalue.r1;
			d->slot_used = rvalue.r2;
			ret = d->ret = rvalue.r3;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(_slbc_request_cache_scmi);

int _slbc_release_cache_scmi(void *ptr)
{
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	slbc_ipi_d.cmd = SLBC_IPI(IPI_SLBC_CACHE_RELEASE_FROM_AP, d->uid);
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_CACHE) {
		ret = slbc_scmi_get(&slbc_ipi_d, 1, &rvalue);
		if (!ret) {
			d->paddr = (void __iomem *)(long long)rvalue.r1;
			d->slot_used = rvalue.r2;
			ret = d->ret = rvalue.r3;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(_slbc_release_cache_scmi);

int _slbc_request_buffer_scmi(void *ptr)
{
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	slbc_ipi_d.cmd = SLBC_IPI(IPI_SLBC_BUFFER_REQUEST_FROM_AP, d->uid);
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_BUFFER) {
		ret = slbc_scmi_set(&slbc_ipi_d, 2);
		if (ret) {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;

			return ret;
		}

		ret = slbc_scmi_get(&slbc_ipi_d, 1, &rvalue);
		if (!ret) {
			d->paddr = (void __iomem *)(long long)rvalue.r1;
			d->slot_used = rvalue.r2;
			ret = d->ret = rvalue.r3;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(_slbc_request_buffer_scmi);

int _slbc_release_buffer_scmi(void *ptr)
{
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	slbc_ipi_d.cmd = SLBC_IPI(IPI_SLBC_BUFFER_RELEASE_FROM_AP, d->uid);
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_BUFFER) {
		ret = slbc_scmi_set(&slbc_ipi_d, 2);
		if (ret) {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;

			return ret;
		}

		ret = slbc_scmi_get(&slbc_ipi_d, 1, &rvalue);
		if (!ret) {
			d->paddr = (void __iomem *)(long long)rvalue.r1;
			d->slot_used = rvalue.r2;
			ret = d->ret = rvalue.r3;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(_slbc_release_buffer_scmi);

static void slbc_scmi_handler(u32 r_feature_id, scmi_tinysys_report *report)
{
	struct slbc_data d;
	unsigned int cmd;
	unsigned int arg;

	if (scmi_slbc_id != r_feature_id)
		return;

	cmd = report->p1;
	arg = report->p2;
	/* pr_info("#@# %s(%d) report 0x%x 0x%x 0x%x 0x%x\n", __func__, __LINE__, */
			/* report->p1, report->p2, report->p3, report->p4); */

	switch (cmd) {
	case IPI_SLBC_SYNC_TO_AP:
		break;
	case IPI_SLBC_ACP_REQUEST_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_ACP) {
			if (ipi_ops && ipi_ops->slbc_request_acp)
				ipi_ops->slbc_request_acp(&d);
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_ACP_REQUEST_TO_AP",
					d.type);
		break;
	case IPI_SLBC_ACP_RELEASE_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_ACP) {
			if (ipi_ops && ipi_ops->slbc_release_acp)
				ipi_ops->slbc_release_acp(&d);
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_ACP_RELEASE_TO_AP",
					d.type);
		break;
	case IPI_SLBC_MEM_BARRIER:
		if (ipi_ops && ipi_ops->slbc_mem_barrier)
			ipi_ops->slbc_mem_barrier();
		break;
	default:
		pr_info("wrong slbc IPI command: %d\n",
				cmd);
	}
}

int slbc_scmi_set(void *buffer, int slot)
{
	int ret;
	struct slbc_ipi_data *slbc_ipi_d = buffer;

	if (!slbc_enable) {
		pr_info("slbc not ready, skip cmd=%d\n", slbc_ipi_d->cmd);
		goto error;
	}

	if (slbc_sspm_ready != 1) {
		pr_info("slbc scmi not ready, skip cmd=%d\n", slbc_ipi_d->cmd);
		goto error;
	}

	/* pr_info("#@# %s(%d) id 0x%x cmd 0x%x arg 0x%x\n", */
			/* __func__, __LINE__, */
			/* scmi_slbc_id, slbc_ipi_d->cmd, slbc_ipi_d->arg); */

	slbc_sram_write(SLBC_SCMI_AP, ++scmi_id);

	ret = scmi_tinysys_common_set(_tinfo->ph, scmi_slbc_id,
			slbc_ipi_d->cmd, slbc_ipi_d->arg, 0, 0, 0);

	if (ret == -ETIMEDOUT) {
		if (scmi_id == slbc_sram_read(SLBC_SCMI_SSPM)) {
			ret = 0;
			pr_info("slbc scmi timed out!\n");
		}
	}

	if (ret) {
		pr_info("slbc scmi cmd %d send fail, ret = %d\n",
				slbc_ipi_d->cmd, ret);

		goto error;
	}

	return ret;
error:
	return -1;
}

int slbc_scmi_get(void *buffer, int slot, void *ptr)
{
	int ret;
	struct slbc_ipi_data *slbc_ipi_d = buffer;
	struct scmi_tinysys_status *rvalue = ptr;

	if (!slbc_enable) {
		pr_info("slbc not ready, skip cmd=%d\n", slbc_ipi_d->cmd);
		goto error;
	}

	if (slbc_sspm_ready != 1) {
		pr_info("slbc scmi not ready, skip cmd=%d\n", slbc_ipi_d->cmd);
		goto error;
	}

	/* pr_info("#@# %s(%d) id 0x%x cmd 0x%x arg 0x%x\n", */
			/* __func__, __LINE__, */
			/* scmi_slbc_id, slbc_ipi_d->cmd, slbc_ipi_d->arg); */

	slbc_sram_write(SLBC_SCMI_AP, ++scmi_id);

	ret = scmi_tinysys_common_get(_tinfo->ph, scmi_slbc_id,
			slbc_ipi_d->cmd, rvalue);

	if (ret == -ETIMEDOUT) {
		if (scmi_id == slbc_sram_read(SLBC_SCMI_SSPM)) {
			ret = 0;
			rvalue->r1 = slbc_sram_read(SLBC_SCMI_RET1);
			rvalue->r2 = slbc_sram_read(SLBC_SCMI_RET2);
			rvalue->r3 = slbc_sram_read(SLBC_SCMI_RET3);
			pr_info("slbc scmi timed out! return 0x%x 0x%x 0x%x\n",
					rvalue->r1,
					rvalue->r2,
					rvalue->r3);
		}
	}

	if (ret) {
		pr_info("slbc scmi cmd %d send fail, ret = %d\n",
				slbc_ipi_d->cmd, ret);

		goto error;
	}

	return ret;
error:
	return -1;
}

int slbc_scmi_init(void)
{
	unsigned int ret;

	_tinfo = get_scmi_tinysys_info();

	if (!(_tinfo && _tinfo->sdev)) {
		pr_info("slbc call get_scmi_tinysys_info() fail\n");
		return -EPROBE_DEFER;
	}

	ret = of_property_read_u32(_tinfo->sdev->dev.of_node, "scmi_slbc",
			&scmi_slbc_id);
	if (ret) {
		pr_info("get slbc scmi_slbc fail, ret %d\n", ret);
		slbc_sspm_ready = -2;
		return -EINVAL;
	}
	pr_info("#@# %s(%d) scmi_slbc_id %d\n",
			__func__, __LINE__, scmi_slbc_id);

	scmi_tinysys_register_event_notifier(scmi_slbc_id,
			(f_handler_t)slbc_scmi_handler);

	ret = scmi_tinysys_event_notify(scmi_slbc_id, 1);

	if (ret) {
		pr_info("event notify fail ...");
		return -EINVAL;
	}

	slbc_sspm_ready = 1;

	pr_info("slbc scmi is ready!\n");

	return 0;
}
EXPORT_SYMBOL_GPL(slbc_scmi_init);

void slbc_register_ipi_ops(struct slbc_ipi_ops *ops)
{
	ipi_ops = ops;
}
EXPORT_SYMBOL_GPL(slbc_register_ipi_ops);

void slbc_unregister_ipi_ops(struct slbc_ipi_ops *ops)
{
	ipi_ops = NULL;
}
EXPORT_SYMBOL_GPL(slbc_unregister_ipi_ops);

MODULE_DESCRIPTION("SLBC scmi Driver v0.1");
MODULE_LICENSE("GPL");
