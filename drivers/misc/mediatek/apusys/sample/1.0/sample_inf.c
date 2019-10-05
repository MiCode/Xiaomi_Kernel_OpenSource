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

#include "../../include/apusys_device.h"
#include "sample_cmn.h"
#include "sample_inf.h"
#include "sample_drv.h"

#define SAMPLE_DEVICE_NUM 2

/* sample driver's private structure */
struct sample_dev_info {
	struct apusys_device *dev;

	uint32_t idx; // core idx
	char name[32];
};

static struct sample_dev_info *sample_private[SAMPLE_DEVICE_NUM];

static void _print_private(void *private)
{
	struct sample_dev_info *info = NULL;

	if (private == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	info = (struct sample_dev_info *)private;
	LOG_INFO("=============================");
	LOG_INFO(" sample driver private info\n");
	LOG_INFO("-----------------------------");
	LOG_INFO(" private ptr = %p\n", info);
	LOG_INFO(" idx         = %d\n", info->idx);
	LOG_INFO(" name        = %s\n", info->name);
	LOG_INFO("=============================");

}

static void _print_hnd(int type, void *hnd)
{
	struct apusys_cmd_hnd *cmd = NULL;
	struct apusys_power_hnd *pwr = NULL;
	struct apusys_preempt_hnd *pmt = NULL;

	/* check argument */
	if (hnd == NULL) {
		LOG_ERR("wrong hnd\n");
		return;
	}

	LOG_INFO("=============================");

	/* print */
	switch (type) {
	case APUSYS_CMD_POWERON:
		pwr = (struct apusys_power_hnd *)hnd;
		LOG_INFO(" power on hnd\n");
		LOG_INFO("-----------------------------");
		LOG_INFO(" opp      = %d\n", pwr->opp);
		LOG_INFO(" boostval = %d\n", pwr->boost_val);
		break;

	case APUSYS_CMD_EXECUTE:
		cmd = (struct apusys_cmd_hnd *)hnd;
		LOG_INFO(" cmd hnd\n");
		LOG_INFO("-----------------------------");
		LOG_INFO(" kva      = 0x%llx\n", cmd->kva);
		LOG_INFO(" iova     = 0x%x\n", cmd->iova);
		LOG_INFO(" size     = %d\n", cmd->iova);
		LOG_INFO(" boostval = %d\n", cmd->boost_val);
		break;

	case APUSYS_CMD_PREEMPT:
		pmt = (struct apusys_preempt_hnd *)hnd;
		LOG_INFO(" pmt hnd\n");
		LOG_INFO("-----------------------------");
		LOG_INFO(" <new cmd>\n");
		LOG_INFO(" kva      = 0x%llx\n", pmt->new_cmd->kva);
		LOG_INFO(" iova     = 0x%x\n", pmt->new_cmd->iova);
		LOG_INFO(" size     = %d\n", pmt->new_cmd->iova);
		LOG_INFO(" boostval = %d\n", pmt->new_cmd->boost_val);
		LOG_INFO(" <old cmd>\n");
		LOG_INFO(" kva      = 0x%llx\n", pmt->old_cmd->kva);
		LOG_INFO(" iova     = 0x%x\n", pmt->old_cmd->iova);
		LOG_INFO(" size     = %d\n", pmt->old_cmd->size);
		LOG_INFO(" boostval = %d\n", pmt->old_cmd->boost_val);
		break;

	default:
		LOG_INFO(" type(%d) hnd\n", type);
		break;
	}
	LOG_INFO("=============================");

}

//----------------------------------------------
static int _sample_poweron(struct apusys_power_hnd *hnd)
{
	if (hnd == NULL)
		return -EINVAL;

	return 0;
}

static int _sample_powerdown(void)
{
	return 0;
}

static int _sample_resume(void)
{
	return 0;
}

static int _sample_suspend(void)
{
	return 0;
}

static int _sample_execute(struct apusys_cmd_hnd *hnd,
	struct apusys_device *dev)
{
	struct sample_request *req = NULL;
	struct sample_dev_info *info = NULL;

	DEBUG_TAG;

	if (hnd == NULL || dev == NULL)
		return -EINVAL;

	DEBUG_TAG;

	/* check cmd */
	if (hnd->kva == 0 || hnd->size == 0 ||
		hnd->size != sizeof(struct sample_request)) {
		LOG_ERR("execute command invalid(0x%llx/%d/%d)\n",
			hnd->kva, hnd->size, sizeof(struct sample_request));
		return -EINVAL;
	};

	req = (struct sample_request *)hnd->kva;
	info = (struct sample_dev_info *)dev->private;

	LOG_INFO("|====================================================|\n");
	LOG_INFO("| sample driver request (use #%-2d device)             |\n",
		info->idx);
	LOG_INFO("|----------------------------------------------------|\n");
	LOG_INFO("| name     = %-32s        |\n",
		req->name);
	LOG_INFO("| algo id  = 0x%-16x                      |\n",
		req->algo_id);
	LOG_INFO("| delay ms = %-16d                        |\n",
		req->delay_ms);
	LOG_INFO("| driver done(should be 0) = %-2d                      |\n",
		req->driver_done);
	LOG_INFO("|====================================================|\n");

	if (req->delay_ms) {
		LOG_INFO("delay %d ms\n", req->delay_ms);
		msleep(req->delay_ms);
	}

	if (req->driver_done != 0) {
		LOG_WARN("driver done flag is (%d)\n", req->driver_done);
		return -EINVAL;
	}

	req->driver_done = 1;

	return 0;
}

static int _sample_preempt(struct apusys_preempt_hnd *hnd)
{
	if (hnd == NULL)
		return -EINVAL;

	return 0;
}

//----------------------------------------------
int sample_send_cmd(int type, void *hnd, struct apusys_device *dev)
{
	int ret = 0;

	LOG_INFO("send cmd: private ptr = %p\n", dev->private);

	_print_private(dev);
	_print_hnd(type, hnd);

	switch (type) {
	case APUSYS_CMD_POWERON:
		LOG_INFO("cmd poweron");
		ret = _sample_poweron(hnd);
		break;

	case APUSYS_CMD_POWERDOWN:
		LOG_INFO("cmd powerdown");
		ret = _sample_powerdown();
		break;

	case APUSYS_CMD_RESUME:
		LOG_INFO("cmd resume");
		ret = _sample_resume();
		break;

	case APUSYS_CMD_SUSPEND:
		LOG_INFO("cmd suspend");
		ret = _sample_suspend();
		break;

	case APUSYS_CMD_EXECUTE:
		LOG_INFO("cmd execute");
		ret = _sample_execute(hnd, dev);
		break;

	case APUSYS_CMD_PREEMPT:
		LOG_INFO("cmd preempt");
		ret = _sample_preempt(hnd);
		break;

	default:
		LOG_ERR("unknown cmd\n");
		ret = -EINVAL;
		break;
	}

	if (ret) {
		LOG_ERR("sample driver send cmd fail, %d,(%d/%p/%p)\n",
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
			LOG_ERR("can't allocate sample info\n");
			ret = -ENOMEM;
			goto alloc_info_fail;
		}

		LOG_INFO("private ptr = %p\n", sample_private[i]);

		/* allocate sample device */
		sample_private[i]->dev =
			kzalloc(sizeof(struct apusys_device), GFP_KERNEL);
		if (sample_private[i]->dev == NULL) {
			LOG_ERR("can't allocate sample dev\n");
			ret = -ENOMEM;
			goto alloc_dev_fail;
		}

		LOG_INFO("sample_dev ptr = %p\n", sample_private[i]->dev);

		/* assign private info */
		sample_private[i]->idx = 0;
		snprintf(sample_private[i]->name, 21, "apusys sample driver");

		/* assign sample dev */
		sample_private[i]->dev->dev_type = APUSYS_DEVICE_SAMPLE;
		sample_private[i]->dev->preempt_type = APUSYS_PREEMPT_NONE;
		sample_private[i]->dev->preempt_level = 0;
		sample_private[i]->dev->private = sample_private[i];
		sample_private[i]->dev->send_cmd = sample_send_cmd;
		sample_private[i]->dev->idx = i;
		sample_private[i]->idx = i;

		/* register device to midware */
		if (apusys_register_device(sample_private[i]->dev)) {
			LOG_ERR("register sample dev fail\n");
			ret = -EINVAL;
			goto register_device_fail;
		}
	}

	return ret;

register_device_fail:
	kfree(sample_private[i]->dev);
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
			LOG_ERR("unregister sample dev fail\n");
			return -EINVAL;
		}

		kfree(sample_private[i]->dev);
		kfree(sample_private[i]);
	}


	return 0;
}
