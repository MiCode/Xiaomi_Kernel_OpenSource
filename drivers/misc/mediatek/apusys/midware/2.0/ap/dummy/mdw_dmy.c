// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/time.h>

#include "apusys_device.h"
#include "mdw_cmn.h"
#include "mdw_dmy.h"

#define MDW_DMY_DEV_NUM 2
#define MDW_DMY_UCMD_IDX 0x66
#define MDW_DMY_UCMD_MAGIC 0x15556
#define MDW_DMY_UCMD_UW 0x1234
#define MDW_DMY_META_DATA "0x15556"
#define MDW_DMY_REQ_NAME_SIZE 32

struct mdw_dmy_req {
	char name[MDW_DMY_REQ_NAME_SIZE];
	uint32_t algo_id;
	uint32_t delay_ms;
	uint8_t driver_done;
};

/* sample driver's private structure */
struct mdw_dmy_dev_info {
	struct apusys_device dev;
	uint32_t idx; // core idx
	char name[32];
	int run;
	uint32_t pwr_status;
	struct mutex mtx;
};

struct mdw_dmy_ucmd {
	unsigned long long magic;
	int cmd_idx;

	int u_write;
};

static struct mdw_dmy_dev_info mdw_dmy_inst[MDW_DMY_DEV_NUM];

static void mdw_dmy_print_hnd(int type, void *hnd)
{
	struct apusys_cmd_handle *cmd = NULL;
	struct apusys_power_hnd *pwr = NULL;
	struct apusys_usercmd_hnd *u = NULL;

	/* check argument */
	if (hnd == NULL)
		return;

	/* print */
	switch (type) {
	case APUSYS_CMD_POWERON:
	case APUSYS_CMD_POWERDOWN:
		pwr = (struct apusys_power_hnd *)hnd;
		mdw_sub_debug("pwr hnd: opp(%u) boost(%d)\n",
			pwr->opp, pwr->boost_val);
		break;

	case APUSYS_CMD_EXECUTE:
		cmd = (struct apusys_cmd_handle *)hnd;
		mdw_sub_debug("cmd hnd: boost(%u)\n", cmd->boost);
		break;

	case APUSYS_CMD_USER:
		u = (struct apusys_usercmd_hnd *)hnd;
		mdw_sub_debug("usr hnd: (0x%llx/0x%llx/%u)\n",
			u->kva, u->iova, u->size);
		break;

	default:
		mdw_sub_debug("hnd(%d) not support\n", type);
		break;
	}
}

//----------------------------------------------
static uint32_t mdw_dmy_get_time_diff(struct timespec64 *duration)
{
	struct timespec64 now;
	uint32_t diff = 0;

	ktime_get_ts64(&now);
	diff = (now.tv_sec - duration->tv_sec)*1000000000 +
		(now.tv_nsec - duration->tv_nsec); //ns
	if (diff)
		diff = diff/1000; //us
	duration->tv_sec = now.tv_sec;
	duration->tv_nsec = now.tv_nsec;

	return diff;
}

//----------------------------------------------
static int mdw_dmy_pwron(struct apusys_power_hnd *hnd,
	struct mdw_dmy_dev_info *info)
{
	if (hnd == NULL || info == NULL)
		return -EINVAL;

	mdw_sub_debug("poweron(%d)\n", info->pwr_status);
	if (hnd->timeout == 0) {
		if (info->pwr_status != 0)
			mdw_drv_err("pwr on already w/o timeout\n");
	}

	info->pwr_status = 1;
	/* if timeout !=0, powerdown cause delay poweroff */
	if (hnd->timeout != 0)
		info->pwr_status = 0;

	return 0;
}

static int mdw_dmy_pwroff(struct mdw_dmy_dev_info *info)
{
	if (info == NULL)
		return -EINVAL;

	mdw_sub_debug("poweroff(%d)\n", info->pwr_status);
	info->pwr_status = 0;

	return 0;
}

static int mdw_dmy_resume(void)
{
	mdw_sub_debug("resume done\n");

	return 0;
}

static int mdw_dmy_suspend(void)
{
	mdw_sub_debug("suspend done\n");

	return 0;
}

static int mdw_dmy_exec(struct apusys_cmd_handle *hnd,
	struct apusys_device *dev)
{
	struct mdw_dmy_req *req = NULL;
	struct mdw_dmy_dev_info *info = NULL;
	struct timespec64 duration;
	uint32_t tdiff = 0;

	if (hnd == NULL || dev == NULL)
		return -EINVAL;

	if (hnd->num_cmdbufs != 1) {
		mdw_drv_err("command num invalid(%u)\n",
			hnd->num_cmdbufs);
		return -EINVAL;
	}

	if (hnd->cmdbufs[0].size != sizeof(struct mdw_dmy_req)) {
		mdw_drv_err("command size invalid(%u)\n",
			hnd->cmdbufs[0].size);
		return -EINVAL;
	}

	mdw_sub_debug("multicore_total = %u\n", hnd->multicore_total);
	req = (struct mdw_dmy_req *)hnd->cmdbufs[0].kva;
	info = (struct mdw_dmy_dev_info *)dev->private;
	mutex_lock(&info->mtx);
	if (info->run != 0) {
		mdw_drv_err("device is occupied\n");
		mutex_unlock(&info->mtx);
		return -EINVAL;
	}
	info->run = 1;

	mdw_sub_debug("dmy-#%u request(0x%llx) (%s/0x%x/%u/%u)\n",
		info->idx, (uint64_t)req, req->name,
		req->algo_id, req->delay_ms, req->driver_done);

	memset(&duration, 0, sizeof(duration));
	tdiff = mdw_dmy_get_time_diff(&duration);

	if (req->delay_ms) {
		mdw_sub_debug("delay %u ms\n", req->delay_ms);
		msleep(req->delay_ms);
	}

	tdiff = mdw_dmy_get_time_diff(&duration);
	hnd->ip_time = tdiff;

	if (req->driver_done != 0) {
		mdw_drv_warn("done flag is (%d)\n", req->driver_done);
		info->run = 0;
		mutex_unlock(&info->mtx);
		return -EINVAL;
	}
	info->run = 0;
	req->driver_done = 1;
	mutex_unlock(&info->mtx);

	return 0;
}

static int mdw_dmy_usr_cmd(void *hnd,
	struct mdw_dmy_dev_info *info)
{
	struct apusys_usercmd_hnd *u = NULL;
	struct mdw_dmy_ucmd *s = NULL;
	int ret = 0;

	if (hnd == NULL || info == NULL)
		return -EINVAL;

	u = (struct apusys_usercmd_hnd *)hnd;

	/* check hnd */
	if (u->kva == 0 || u->size == 0) {
		mdw_drv_err("invalid argument(0x%llx/0x%x/%u)\n",
			u->kva, u->iova, u->size);
		return -EINVAL;
	}

	/* check cmd size */
	if (u->size != sizeof(struct mdw_dmy_ucmd)) {
		mdw_drv_err("handle size not match(%u/%lu)\n",
			u->size, sizeof(struct mdw_dmy_ucmd));
		return -EINVAL;
	}

	/* verify param sent from user space */
	s = (struct mdw_dmy_ucmd *)u->kva;
	if (s->cmd_idx != MDW_DMY_UCMD_IDX ||
		s->magic != MDW_DMY_UCMD_MAGIC) {
		mdw_drv_err("user cmd param not match(%d/0x%llx)\n",
			s->cmd_idx, s->magic);
		return -EINVAL;
	}

	s->u_write = MDW_DMY_UCMD_UW;
	mdw_sub_debug("get user cm ok\n");

	return ret;
}

//----------------------------------------------
static int mdw_dmy_send_cmd(int type, void *hnd, struct apusys_device *dev)
{
	int ret = 0;

	mdw_sub_debug("send cmd: private ptr = %p\n", dev->private);

	mdw_dmy_print_hnd(type, hnd);

	switch (type) {
	case APUSYS_CMD_POWERON:
		mdw_sub_debug("cmd poweron\n");
		ret = mdw_dmy_pwron(hnd,
			(struct mdw_dmy_dev_info *)dev->private);
		break;

	case APUSYS_CMD_POWERDOWN:
		mdw_sub_debug("cmd powerdown\n");
		ret = mdw_dmy_pwroff((struct mdw_dmy_dev_info *)dev->private);
		break;

	case APUSYS_CMD_RESUME:
		mdw_sub_debug("cmd resume\n");
		ret = mdw_dmy_resume();
		break;

	case APUSYS_CMD_SUSPEND:
		mdw_sub_debug("cmd suspend\n");
		ret = mdw_dmy_suspend();
		break;

	case APUSYS_CMD_EXECUTE:
		mdw_sub_debug("cmd execute\n");
		ret = mdw_dmy_exec(hnd, dev);
		break;

	case APUSYS_CMD_USER:
		ret = mdw_dmy_usr_cmd(hnd,
			(struct mdw_dmy_dev_info *)dev->private);
		break;

	default:
		mdw_drv_err("unknown cmd(%d)\n", type);
		ret = -EINVAL;
		break;
	}

	if (ret) {
		mdw_drv_err("send cmd fail, %d (%d/%p/%p)\n",
			ret, type, hnd, dev);
	}

	return ret;
}

int mdw_dmy_init(void)
{
	int ret = 0, i = 0, n = 0;

	for (i = 0; i < MDW_DMY_DEV_NUM; i++) {
		/* assign private info */
		mdw_dmy_inst[i].idx = i;
		if (snprintf(mdw_dmy_inst[i].name, 21,
			"apusys sample driver") < 0)
			goto delete_dev;

		/* assign sample dev */
		mdw_dmy_inst[i].dev.dev_type = APUSYS_DEVICE_SAMPLE;
		mdw_dmy_inst[i].dev.preempt_type = APUSYS_PREEMPT_NONE;
		mdw_dmy_inst[i].dev.preempt_level = 0;
		n = snprintf(mdw_dmy_inst[i].dev.meta_data,
			sizeof(mdw_dmy_inst[i].dev.meta_data),
			MDW_DMY_META_DATA);
		if (n < 0 || n >= sizeof(mdw_dmy_inst[i].dev.meta_data))
			goto delete_dev;
		mdw_dmy_inst[i].dev.private = &mdw_dmy_inst[i];
		mdw_dmy_inst[i].dev.send_cmd = mdw_dmy_send_cmd;
		mdw_dmy_inst[i].dev.idx = i;
		mdw_dmy_inst[i].idx = i;

		mutex_init(&mdw_dmy_inst[i].mtx);

		/* register device to midware */
		if (apusys_register_device(&mdw_dmy_inst[i].dev)) {
			mdw_drv_err("register dev fail\n");
			ret = -EINVAL;
			goto delete_dev;
		}
	}

	goto out;

delete_dev:
	memset(mdw_dmy_inst, 0, sizeof(mdw_dmy_inst));
out:
	return ret;
}

void mdw_dmy_deinit(void)
{
	memset(mdw_dmy_inst, 0, sizeof(mdw_dmy_inst));
}
