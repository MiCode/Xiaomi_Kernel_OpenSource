/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h> /* copy_from/to_user() */

#include <v1/sspm_ipi.h>
#include <trace/events/mtk_events.h>

#include <mtk_sspm.h>
#include <mtk_spm_internal.h>


#define SPM_D_LEN   (8) /* # of cmd + arg0 + arg1 + ... */

int spm_to_sspm_command_async(u32 cmd, struct spm_data *spm_d)
{
	unsigned int ret = 0;

	switch (cmd) {
	case SPM_DPIDLE_ENTER:
	case SPM_DPIDLE_LEAVE:
	case SPM_ENTER_SODI:
	case SPM_LEAVE_SODI:
	case SPM_ENTER_SODI3:
	case SPM_LEAVE_SODI3:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_async(
			IPI_ID_SPM_SUSPEND, IPI_OPT_DEFAUT, spm_d, SPM_D_LEN);
		if (ret != 0)
			pr_notice("#@# %s(%d) sspm_ipi_send_async(cmd:0x%x) ret %d\n",
				__func__, __LINE__, cmd, ret);
		break;
	default:
		pr_notice("#@# %s(%d) cmd(%d) wrong!!!\n", __func__,
			__LINE__, cmd);
		break;
	}

	return ret;
}

int spm_to_sspm_command_async_wait(u32 cmd)
{
	int ack_data;
	unsigned int ret = 0;

	switch (cmd) {
	case SPM_DPIDLE_ENTER:
	case SPM_DPIDLE_LEAVE:
	case SPM_ENTER_SODI:
	case SPM_LEAVE_SODI:
	case SPM_ENTER_SODI3:
	case SPM_LEAVE_SODI3:
		ret = sspm_ipi_send_async_wait(
			IPI_ID_SPM_SUSPEND, IPI_OPT_DEFAUT, &ack_data);
		if (ret != 0) {
			pr_notice("#@# %s(%d) sspm_ipi_send_async_wait(cmd:0x%x) ret %d\n",
				__func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_notice("#@# %s(%d) cmd(%d) return %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;
	default:
		pr_notice("#@# %s(%d) cmd(%d) wrong!!!\n", __func__,
			__LINE__, cmd);
		break;
	}

	return ret;
}

int spm_to_sspm_command(u32 cmd, struct spm_data *spm_d)
{
	int ack_data;
	unsigned int ret = 0;
	/* struct spm_data _spm_d; */

	switch (cmd) {
	case SPM_SUSPEND:
	case SPM_RESUME:
	case SPM_DPIDLE_ENTER:
	case SPM_DPIDLE_LEAVE:
	case SPM_ENTER_SODI:
	case SPM_ENTER_SODI3:
	case SPM_LEAVE_SODI:
	case SPM_LEAVE_SODI3:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(
			IPI_ID_SPM_SUSPEND, IPI_OPT_POLLING, spm_d, SPM_D_LEN,
				&ack_data, 1);
		if (ret != 0) {
			pr_notice("#@# %s(%d) cmd:0x%x ret %d\n",
				__func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_notice("#@# %s(%d) cmd:0x%x ret %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_VCORE_PWARP_CMD:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(
			IPI_ID_SPM_SUSPEND, IPI_OPT_POLLING, spm_d, SPM_D_LEN,
				&ack_data, 1);
		if (ret != 0) {
			pr_notice("#@# %s(%d) cmd:0x%x ret %d\n",
				__func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_notice("#@# %s(%d) cmd:0x%x ret %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_SUSPEND_PREPARE:
	case SPM_POST_SUSPEND:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(
			IPI_ID_SPM_SUSPEND, IPI_OPT_POLLING, spm_d, SPM_D_LEN,
				&ack_data, 1);
		if (ret != 0) {
			pr_notice("#@# %s(%d) cmd:0x%x ret %d\n",
				__func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_notice("#@# %s(%d) cmd:0x%x ret %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_DPIDLE_PREPARE:
	case SPM_POST_DPIDLE:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(
			IPI_ID_SPM_SUSPEND, IPI_OPT_POLLING, spm_d, SPM_D_LEN,
				&ack_data, 1);
		if (ret != 0) {
			pr_notice("#@# %s(%d) cmd:0x%x ret %d\n",
				__func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_notice("#@# %s(%d) cmd:0x%x ret %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;
	case SPM_SODI_PREPARE:
	case SPM_POST_SODI:
	case SPM_TWAM_ENABLE:
		spm_d->cmd = cmd;
		ret = sspm_ipi_send_sync(
			IPI_ID_SPM_SUSPEND, IPI_OPT_POLLING, spm_d, SPM_D_LEN,
				&ack_data, 1);
		if (ret != 0) {
			pr_notice("#@# %s(%d) cmd:0x%x ret %d\n",
				__func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_notice("#@# %s(%d) cmd:0x%x ret %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;
	default:
		pr_notice("#@# %s(%d) cmd(%d) wrong!!!\n",
			__func__, __LINE__, cmd);
		break;
	}

	return ret;
}

static atomic_t ipi_lock_cnt;

bool is_sspm_ipi_lock_spm(void)
{
	int lock_cnt = -1;
	bool ret = false;

	lock_cnt = atomic_read(&ipi_lock_cnt);

	ret = (lock_cnt == 0) ? false : true;

	return ret;
}

void sspm_ipi_lock_spm_scenario(int start, int id, int opt, const char *name)
{
	if (id == IPI_ID_SPM_SUSPEND)
		return;

	if (id < 0 || id >= IPI_ID_TOTAL)
		return;

	if (start)
		atomic_inc(&ipi_lock_cnt);
	else
		atomic_dec(&ipi_lock_cnt);

	/* FTRACE tag */
	trace_sspm_ipi(start, id, opt);
}

