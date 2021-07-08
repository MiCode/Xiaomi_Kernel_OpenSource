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

static phys_addr_t mem_phys_addr, mem_virt_addr;
static unsigned long long mem_size;
static int slbc_sspm_ready;
static int slbc_ipi_enable = 1;
static int scmi_slbc_id;
static struct scmi_tinysys_info_st *_tinfo;

int slbc_get_ipi_enable(void)
{
	return slbc_ipi_enable;
}
EXPORT_SYMBOL(slbc_get_ipi_enable);

void slbc_set_ipi_enable(int enable)
{
	slbc_ipi_enable = enable;
}
EXPORT_SYMBOL(slbc_set_ipi_enable);

void slbc_sspm_enable(enable)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_ENABLE;
	slbc_ipi_d.arg = enable;
	slbc_ipi_to_sspm_command(&slbc_ipi_d, 2);
}
EXPORT_SYMBOL(slbc_sspm_enable);

/* FIXME: */
/* fix slbc_ipi->mtk_slbc->slbc_ipi case */
void slbc_request_cache(struct slbc_data *d)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_CACHE_REQUEST_FROM_AP;
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_CACHE)
		slbc_ipi_to_sspm_command(&slbc_ipi_d, 2);
	else
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
}

void slbc_release_cache(struct slbc_data *d)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_CACHE_RELEASE_FROM_AP;
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_CACHE)
		slbc_ipi_to_sspm_command(&slbc_ipi_d, 2);
	else
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
}

void slbc_request_buffer(struct slbc_data *d)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_BUFFER_REQUEST_FROM_AP;
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_BUFFER)
		slbc_ipi_to_sspm_command(&slbc_ipi_d, 2);
	else
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
}

void slbc_release_buffer(struct slbc_data *d)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_BUFFER_RELEASE_FROM_AP;
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_BUFFER)
		slbc_ipi_to_sspm_command(&slbc_ipi_d, 2);
	else
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
}

void slbc_request_acp(struct slbc_data *d)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_ACP_REQUEST_FROM_AP;
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_ACP)
		slbc_ipi_to_sspm_command(&slbc_ipi_d, 2);
	else
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
}

void slbc_release_acp(struct slbc_data *d)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_ACP_RELEASE_FROM_AP;
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_ACP)
		slbc_ipi_to_sspm_command(&slbc_ipi_d, 2);
	else
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
}

static void slbc_scmi_handler(u32 r_feature_id, scmi_tinysys_report *report)
{
	struct slbc_data d;
	unsigned int cmd;
	unsigned int arg;

	if (scmi_slbc_id != r_feature_id)
		return;

	cmd = report->p1;
	arg = report->p2;
	pr_info("#@# %s(%d) report 0x%x 0x%x 0x%x 0x%x\n", __func__, __LINE__,
			report->p1, report->p2, report->p3, report->p4);

	switch (cmd) {
	case IPI_SLBC_SYNC_TO_AP:
		break;
	case IPI_SLBC_CACHE_REQUEST_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_CACHE) {
			/* FIXME: */
			/* slbc_request(&d); */
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_CACHE_REQUEST_TO_AP",
					d.type);
		break;
	case IPI_SLBC_CACHE_RELEASE_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_CACHE) {
			/* FIXME: */
			/* slbc_release(&d); */
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_CACHE_RELEASE_TO_AP",
					d.type);
		break;
	case IPI_SLBC_BUFFER_REQUEST_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_BUFFER) {
			d.flag |= FG_POWER;
			/* slbc_power_on(&d); */
			/* slbc_request(&d); */
			/* FIXME: */
			/* reply_data = (unsigned short) */
				/* ((((uintptr_t)d.paddr) >> 16) & 0xffff); */
			/* pr_info("#@# %s(%d) reply_data(0x%x) 0x%x\n", */
					/* __func__, __LINE__, */
					/* cmd, reply_data); */
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_BUFFER_REQUEST_TO_AP",
					d.type);
		break;
	case IPI_SLBC_BUFFER_RELEASE_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_BUFFER) {
			d.flag |= FG_POWER;
			/* FIXME: */
			/* slbc_release(&d); */
			/* slbc_power_off(&d); */
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_BUFFER_RELEASE_TO_AP",
					d.type);
		break;
	case IPI_SLBC_ACP_REQUEST_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_ACP) {
			/* FIXME: */
			/* slbc_request(&d); */
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_ACP_REQUEST_TO_AP",
					d.type);
		break;
	case IPI_SLBC_ACP_RELEASE_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_ACP) {
			/* FIXME: */
			/* slbc_release(&d); */
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_ACP_RELEASE_TO_AP",
					d.type);
		break;
	default:
		pr_info("wrong slbc IPI command: %d\n",
				cmd);
	}
}

unsigned int slbc_ipi_to_sspm_command(void *buffer, int slot)
{
	int ret;
	struct slbc_ipi_data *slbc_ipi_d = buffer;
	/* struct scmi_tinysys_status rvalue; */

	if (slbc_sspm_ready != 1) {
		pr_info("slbc ipi not ready, skip cmd=%d\n", slbc_ipi_d->cmd);
		goto error;
	}

	ret = scmi_tinysys_common_set(_tinfo->ph, scmi_slbc_id,
			slbc_ipi_d->cmd, slbc_ipi_d->arg, 0, 0, 0);
	/* ret = scmi_tinysys_common_get(_tinfo->ph, scmi_slbc_id, */
			/* slbc_ipi_d->cmd, &rvalue); */
	/* pr_info("#@# %s(%d) ret %d status 0x%x r 0x%x 0x%x 0x%x\n", */
			/* __func__, __LINE__, ret,rvalue.status, */
			/* rvalue.r1, rvalue.r2, rvalue.r3); */
	if (ret) {
		pr_info("slbc ipi cmd %d send fail, ret = %d\n",
				slbc_ipi_d->cmd, ret);
		goto error;
	}

	return ret;
error:
	return -1;
}
EXPORT_SYMBOL(slbc_ipi_to_sspm_command);

static void slbc_get_rec_addr(phys_addr_t *phys,
		phys_addr_t *virt,
		unsigned long long *size)
{
	/* get sspm reserved mem */
	/* FIXME: */
	/* *phys = sspm_reserve_mem_get_phys(QOS_MEM_ID); */
	/* *virt = sspm_reserve_mem_get_virt(QOS_MEM_ID); */
	/* *size = sspm_reserve_mem_get_size(QOS_MEM_ID); */

	pr_info("phy_addr = 0x%llx, virt_addr=0x%llx, size = %llu\n",
			(unsigned long long) *phys,
			(unsigned long long) *virt,
			*size);
}

static int slbc_reserve_mem_init(phys_addr_t *virt,
		unsigned long long *size)
{
	int i;
	unsigned char *ptr;

	if (!virt)
		return -1;

	/* clear reserve mem */
	ptr = (unsigned char *)(uintptr_t)*virt;
	for (i = 0; i < *size; i++)
		ptr[i] = 0x0;

	return 0;
}

void slbc_suspend_resume_notify(int suspend)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_SUSPEND_RESUME_NOTIFY;
	slbc_ipi_d.arg = suspend;
	slbc_ipi_to_sspm_command(&slbc_ipi_d, 2);
}
EXPORT_SYMBOL(slbc_suspend_resume_notify);

static inline void slbc_pass_to_sspm(void)
{
	struct slbc_ipi_data slbc_ipi_d;

	slbc_ipi_d.cmd = IPI_SLBC_MEM_INIT;
	slbc_ipi_d.arg = (unsigned int)(mem_phys_addr & 0xFFFFFFFF);
	slbc_ipi_to_sspm_command(&slbc_ipi_d, 2);
}

int slbc_ipi_init(void)
{
	unsigned int ret;

	_tinfo = get_scmi_tinysys_info();

	if (!(_tinfo && _tinfo->sdev)) {
		pr_info("call get_scmi_tinysys_info() fail\n");
		return -EPROBE_DEFER;
	}

	ret = of_property_read_u32(_tinfo->sdev->dev.of_node, "scmi_slbc",
			&scmi_slbc_id);
	if (ret) {
		pr_info("get scmi_slbc fail, ret %d\n", ret);
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
	slbc_sspm_enable(slbc_ipi_enable);
	pr_info("slbc ipi is ready!\n");

	slbc_get_rec_addr(&mem_phys_addr, &mem_virt_addr, &mem_size);
	slbc_reserve_mem_init(&mem_virt_addr, &mem_size);
	slbc_pass_to_sspm();

	return 0;
}
EXPORT_SYMBOL(slbc_ipi_init);

MODULE_DESCRIPTION("SLBC ipi Driver v0.1");
MODULE_LICENSE("GPL");
