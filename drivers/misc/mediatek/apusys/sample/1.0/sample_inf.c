/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/time.h>

#include "../../include/apusys_device.h"
#include "sample_cmn.h"
#include "sample_inf.h"
#include "sample_drv.h"

#define SAMPLE_DEVICE_NUM 2

#define SAMPLE_BOOST_MAGIC 87
#define SAMPLE_OPP_MAGIC 7
#define SAMPLE_FW_MAGIC 0x35904
#define SAMPLE_FW_PTN 0x8A

#define SAMPLE_USERCMD_IDX 0x66
#define SAMPLE_USERCMD_MAGIC 0x15556

struct sample_fw {
	char name[32];

	uint64_t kva;
	uint32_t size;
};

/* sample driver's private structure */
struct sample_dev_info {
	struct apusys_device *dev;

	uint32_t idx; // core idx
	char name[32];

	struct sample_fw fw;

	int run;
	uint32_t pwr_status;

	struct mutex mtx;
};

struct sample_usercmd {
	unsigned long long magic;
	int cmd_idx;

	int u_write;
};

static struct sample_dev_info *sample_private[SAMPLE_DEVICE_NUM];
#if 0
void _print_private(void *private)
{
	struct sample_dev_info *info = NULL;

	if (private == NULL) {
		spl_drv_err("invalid argument\n");
		return;
	}

	info = (struct sample_dev_info *)private;
	spl_drv_info("=============================");
	spl_drv_info(" sample driver private info\n");
	spl_drv_info("-----------------------------");
	spl_drv_info(" private ptr = %p\n", info);
	spl_drv_info(" idx         = %d\n", info->idx);
	spl_drv_info(" name        = %s\n", info->name);
	spl_drv_info("=============================");

}
#endif
static void _print_hnd(int type, void *hnd)
{
	struct apusys_cmd_hnd *cmd = NULL;
	struct apusys_power_hnd *pwr = NULL;
	struct apusys_preempt_hnd *pmt = NULL;
	struct apusys_firmware_hnd *fw = NULL;
	struct apusys_usercmd_hnd *u = NULL;

	/* check argument */
	if (hnd == NULL) {
		//spl_drv_err("wrong hnd\n");
		return;
	}

	spl_drv_dbg("================================");

	/* print */
	switch (type) {
	case APUSYS_CMD_POWERON:
	case APUSYS_CMD_POWERDOWN:
		pwr = (struct apusys_power_hnd *)hnd;
		spl_drv_dbg("| power on hnd                 |\n");
		spl_drv_dbg("--------------------------------");
		spl_drv_dbg("| opp      = %-18d|\n", pwr->opp);
		spl_drv_dbg("| boostval = %-18d|\n", pwr->boost_val);
		break;

	case APUSYS_CMD_SUSPEND:
	case APUSYS_CMD_RESUME:
		break;

	case APUSYS_CMD_EXECUTE:
		cmd = (struct apusys_cmd_hnd *)hnd;
		spl_drv_dbg(" cmd hnd                       |\n");
		spl_drv_dbg("--------------------------------");
		spl_drv_dbg("| kva      = 0x%-16llx|\n", cmd->kva);
		spl_drv_dbg("| iova     = 0x%-16x|\n", cmd->iova);
		spl_drv_dbg("| size     = %-18u|\n", cmd->iova);
		spl_drv_dbg("| boostval = %-18d|\n", cmd->boost_val);
		break;

	case APUSYS_CMD_PREEMPT:
		pmt = (struct apusys_preempt_hnd *)hnd;
		spl_drv_dbg("| pmt hnd                      |\n");
		spl_drv_dbg("--------------------------------");
		spl_drv_dbg("| <new cmd>                    |\n");
		spl_drv_dbg("| kva      = 0x%-16llx|\n", pmt->new_cmd->kva);
		spl_drv_dbg("| iova     = 0x%-16x|\n", pmt->new_cmd->iova);
		spl_drv_dbg("| size     = %-18d|\n", pmt->new_cmd->iova);
		spl_drv_dbg("| boostval = %-18d|\n", pmt->new_cmd->boost_val);
		spl_drv_dbg("| <old cmd>                    |\n");
		spl_drv_dbg("| kva      = 0x%-16llx|\n", pmt->old_cmd->kva);
		spl_drv_dbg("| iova     = 0x%-16x|\n", pmt->old_cmd->iova);
		spl_drv_dbg("| size     = %-18u|\n", pmt->old_cmd->size);
		spl_drv_dbg("| boostval = %-18d|\n", pmt->old_cmd->boost_val);
		break;

	case APUSYS_CMD_FIRMWARE:
		fw = (struct apusys_firmware_hnd *)hnd;
		spl_drv_dbg("| fw hnd                      |\n");
		spl_drv_dbg("--------------------------------");
		spl_drv_dbg("| name     = %-18s|\n", fw->name);
		spl_drv_dbg("| magic    = 0x%-16x|\n", fw->magic);
		spl_drv_dbg("| kva      = 0x%-16llx|\n", fw->kva);
		spl_drv_dbg("| iova     = 0x%-16x|\n", fw->iova);
		spl_drv_dbg("| size     = %-18u|\n", fw->size);
		spl_drv_dbg("| idx      = %-18d|\n", fw->idx);
		spl_drv_dbg("| op       = %-18d|\n", fw->op);
		break;

	case APUSYS_CMD_USER:
		u = (struct apusys_usercmd_hnd *)hnd;
		spl_drv_dbg("| user hnd                      |\n");
		spl_drv_dbg("--------------------------------");
		spl_drv_dbg("| kva      = 0x%-16llx|\n", u->kva);
		spl_drv_dbg("| iova     = 0x%-16x|\n", u->iova);
		spl_drv_dbg("| size     = %-18u|\n", u->size);
		break;

	default:
		spl_drv_dbg("| not support type(%-2d) hnd    |\n", type);
		break;
	}
	spl_drv_dbg("================================");

}

//----------------------------------------------
static uint32_t _get_time_diff_from_system(struct timeval *duration)
{
	struct timeval now;
	uint32_t diff = 0;

	do_gettimeofday(&now);
	diff = (now.tv_sec - duration->tv_sec)*1000000 +
		now.tv_usec - duration->tv_usec;
	duration->tv_sec = now.tv_sec;
	duration->tv_usec = now.tv_usec;

	return diff;
}

//----------------------------------------------
static int _sample_poweron(struct apusys_power_hnd *hnd,
	struct sample_dev_info *info)
{
	if (hnd == NULL || info == NULL)
		return -EINVAL;

	spl_drv_dbg("sample poweron(%d)\n", info->pwr_status);
	if (hnd->timeout == 0) {
		if (info->pwr_status != 0)
			spl_drv_err("pwr on already w/o timeout\n");
	}

	info->pwr_status = 1;
	/* if timeout !=0, powerdown cause delay poweroff */
	if (hnd->timeout != 0)
		info->pwr_status = 0;

	return 0;
}

static int _sample_powerdown(struct sample_dev_info *info)
{
	if (info == NULL)
		return -EINVAL;

	spl_drv_dbg("sample poweroff(%d)\n", info->pwr_status);
	info->pwr_status = 0;

	return 0;
}

static int _sample_resume(void)
{
	spl_drv_dbg("sample resume done\n");

	return 0;
}

static int _sample_suspend(void)
{
	spl_drv_dbg("sample suspend done\n");

	return 0;
}

static int _sample_execute(struct apusys_cmd_hnd *hnd,
	struct apusys_device *dev)
{
	struct sample_request *req = NULL;
	struct sample_dev_info *info = NULL;
	struct timeval duration;
	uint32_t tdiff = 0;

	if (hnd == NULL || dev == NULL)
		return -EINVAL;

	/* check cmd */
	if (hnd->kva == 0 || hnd->size == 0 ||
		hnd->size != sizeof(struct sample_request)) {
		spl_drv_err("execute command invalid(0x%llx/%d/%d)\n",
			hnd->kva,
			(int)hnd->size,
			(int)sizeof(struct sample_request));
		return -EINVAL;
	};

	req = (struct sample_request *)hnd->kva;
	info = (struct sample_dev_info *)dev->private;
	mutex_lock(&info->mtx);
	if (info->run != 0) {
		spl_drv_err("device is occupied\n");
		mutex_unlock(&info->mtx);
		return -EINVAL;
	}
	info->run = 1;

	/* memory api test */
	if (hnd->cmdbuf != NULL) {
		spl_drv_dbg("flush memory test\n");
		if (apusys_mem_flush(hnd->cmdbuf))
			spl_drv_err("flush memory fail");
		else
			spl_drv_dbg("flush memory done");

		spl_drv_dbg("invalidate memory test\n");
		if (apusys_mem_invalidate(hnd->cmdbuf))
			spl_drv_err("invalidate memory fail");
		else
			spl_drv_dbg("invalidate memory done");
	} else {
		spl_drv_warn("no cmdbuf\n");
	}

	spl_drv_dbg("|====================================================|\n");
	spl_drv_dbg("| sample driver request (use #%-2d device)             |\n",
		info->idx);
	spl_drv_dbg("|----------------------------------------------------|\n");
	spl_drv_dbg("| name     = %-32s        |\n",
		req->name);
	spl_drv_dbg("| algo id  = 0x%-16x                      |\n",
		req->algo_id);
	spl_drv_dbg("| delay ms = %-16d                        |\n",
		req->delay_ms);
	spl_drv_dbg("| driver done(should be 0) = %-2d                      |\n",
		req->driver_done);
	spl_drv_dbg("|----------------------------------------------------|\n");

	memset(&duration, 0, sizeof(struct timeval));
	tdiff = _get_time_diff_from_system(&duration);

	if (req->delay_ms) {
		spl_drv_dbg("delay %d ms\n", req->delay_ms);
		msleep(req->delay_ms);
	}

	tdiff = _get_time_diff_from_system(&duration);
	spl_drv_dbg("| ip time  = %-16ld                        |\n",
		tdiff);
	spl_drv_dbg("|====================================================|\n");

	hnd->ip_time = tdiff;

	if (req->driver_done != 0) {
		spl_drv_warn("driver done flag is (%d)\n", req->driver_done);
		info->run = 0;
		mutex_unlock(&info->mtx);
		return -EINVAL;
	}
	info->run = 0;
	req->driver_done = 1;
	mutex_unlock(&info->mtx);

	return 0;
}

static int _sample_preempt(struct apusys_preempt_hnd *hnd)
{
	if (hnd == NULL)
		return -EINVAL;

	return 0;
}

static int _sample_firmware(struct apusys_firmware_hnd *hnd,
	struct sample_dev_info *info)
{
	int ret = 0;

	/* check argument */
	if (hnd == NULL || info == NULL)
		return -EINVAL;

	/* check fw magic */
	if (hnd->magic != SAMPLE_FW_MAGIC || hnd->kva == 0
		|| hnd->size == 0) {
		spl_drv_err("apusys sample error(0x%x/0x%llx/0x%x/%d)\n",
			hnd->magic, hnd->kva, hnd->iova, hnd->size);
		return -EINVAL;
	}

	/* execute fw command */
	if (hnd->op == APUSYS_FIRMWARE_LOAD) {
		spl_drv_dbg("load firmware(%s)\n", hnd->name);
		memset((void *)hnd->kva, SAMPLE_FW_PTN, hnd->size);
		strncpy(info->fw.name, hnd->name, sizeof(info->fw.name)-1);
		info->fw.kva = hnd->kva;
		info->fw.size = hnd->size;
	} else {
		spl_drv_dbg("unload firmware(%s)\n", hnd->name);
		memset((void *)info->fw.kva, 0, info->fw.size);
		memset(info->fw.name, 0, sizeof(info->fw.name));
		info->fw.kva = 0;
		info->fw.size = 0;
	}

	return ret;
}

static int _sample_usercmd(void *hnd,
	struct sample_dev_info *info)
{
	struct apusys_usercmd_hnd *u = NULL;
	struct sample_usercmd *s = NULL;
	int ret = 0;

	if (hnd == NULL || info == NULL)
		return -EINVAL;

	u = (struct apusys_usercmd_hnd *)hnd;

	/* check hnd */
	if (u->kva == 0 || u->iova == 0 || u->size == 0) {
		spl_drv_err("invalid argument(0x%llx/0x%x/%u)\n",
			u->kva, u->iova, u->size);
		return -EINVAL;
	}

	/* check cmd size */
	if (u->size != sizeof(struct sample_usercmd)) {
		spl_drv_err("sample handle size not match(%u/%lu)\n",
			u->size, sizeof(struct sample_usercmd));
		return -EINVAL;
	}

	/* verify param sent from user space */
	s = (struct sample_usercmd *)u->kva;
	if (s->cmd_idx != SAMPLE_USERCMD_IDX ||
		s->magic != SAMPLE_USERCMD_MAGIC) {
		spl_drv_err("sample user cmd param not match(%d/0x%llx)\n",
			s->cmd_idx, s->magic);
		return -EINVAL;
	}

	if (info->idx != s->u_write) {
		spl_drv_err("user write error (%d/%d)\n",
			s->u_write, info->idx);
		return -EINVAL;
	}

	spl_drv_dbg("get user cmd: %d ok\n", s->u_write);

	return ret;
}

//----------------------------------------------
int sample_send_cmd(int type, void *hnd, struct apusys_device *dev)
{
	int ret = 0;

	spl_drv_dbg("send cmd: private ptr = %p\n", dev->private);

	_print_hnd(type, hnd);

	switch (type) {
	case APUSYS_CMD_POWERON:
		spl_drv_dbg("cmd poweron\n");
		ret = _sample_poweron(hnd,
			(struct sample_dev_info *)dev->private);
		break;

	case APUSYS_CMD_POWERDOWN:
		spl_drv_dbg("cmd powerdown\n");
		ret = _sample_powerdown((struct sample_dev_info *)dev->private);
		break;

	case APUSYS_CMD_RESUME:
		spl_drv_dbg("cmd resume\n");
		ret = _sample_resume();
		break;

	case APUSYS_CMD_SUSPEND:
		spl_drv_dbg("cmd suspend\n");
		ret = _sample_suspend();
		break;

	case APUSYS_CMD_EXECUTE:
		spl_drv_dbg("cmd execute\n");
		ret = _sample_execute(hnd, dev);
		break;

	case APUSYS_CMD_PREEMPT:
		spl_drv_dbg("cmd preempt\n");
		ret = _sample_preempt(hnd);
		break;

	case APUSYS_CMD_FIRMWARE:
		spl_drv_dbg("cmd firmware\n");
		ret = _sample_firmware(hnd,
			(struct sample_dev_info *)dev->private);
		break;

	case APUSYS_CMD_USER:
		ret = _sample_usercmd(hnd,
			(struct sample_dev_info *)dev->private);
		break;

	default:
		spl_drv_err("unknown cmd\n");
		ret = -EINVAL;
		break;
	}

	if (ret) {
		spl_drv_err("sample driver send cmd fail, %d (%d/%p/%p)\n",
			ret, type, hnd, dev);
	}

	return ret;
}

int sample_device_init(void)
{
	int ret = 0, i = 0;

	for (i = 0; i < SAMPLE_DEVICE_NUM; i++) {
		/* allocate private info */
		sample_private[i] =
			kzalloc(sizeof(struct sample_dev_info), GFP_KERNEL);
		if (sample_private[i] == NULL) {
			spl_drv_err("can't allocate sample info\n");
			ret = -ENOMEM;
			goto alloc_info_fail;
		}

		spl_drv_dbg("private ptr = %p\n", sample_private[i]);

		/* allocate sample device */
		sample_private[i]->dev =
			kzalloc(sizeof(struct apusys_device), GFP_KERNEL);
		if (sample_private[i]->dev == NULL) {
			spl_drv_err("can't allocate sample dev\n");
			ret = -ENOMEM;
			goto alloc_dev_fail;
		}

		spl_drv_dbg("sample_dev ptr = %p\n", sample_private[i]->dev);

		/* assign private info */
		sample_private[i]->idx = 0;
		if (snprintf(sample_private[i]->name, 21,
			"apusys sample driver") < 0)
			goto fail_set_name;

		/* assign sample dev */
		sample_private[i]->dev->dev_type = APUSYS_DEVICE_SAMPLE;
		sample_private[i]->dev->preempt_type = APUSYS_PREEMPT_NONE;
		sample_private[i]->dev->preempt_level = 0;
		sample_private[i]->dev->private = sample_private[i];
		sample_private[i]->dev->send_cmd = sample_send_cmd;
		sample_private[i]->dev->idx = i;
		sample_private[i]->idx = i;

		mutex_init(&sample_private[i]->mtx);

		/* register device to midware */
		if (apusys_register_device(sample_private[i]->dev)) {
			spl_drv_err("register sample dev fail\n");
			ret = -EINVAL;
			goto register_device_fail;
		}
	}

	return ret;

register_device_fail:
	kfree(sample_private[i]->dev);
fail_set_name:
alloc_dev_fail:
	kfree(sample_private[i]);
alloc_info_fail:

	return ret;
}

int sample_device_destroy(void)
{
	int i = 0;

	for (i = SAMPLE_DEVICE_NUM - 1; i >= 0 ; i--) {
		if (apusys_unregister_device(sample_private[i]->dev)) {
			spl_drv_err("unregister sample dev fail\n");
			return -EINVAL;
		}

		kfree(sample_private[i]->dev);
		kfree(sample_private[i]);
	}


	return 0;
}
