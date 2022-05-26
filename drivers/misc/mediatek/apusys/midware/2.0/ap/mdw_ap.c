// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/bitmap.h>
#include "mdw_cmn.h"
#include "mdw_ap.h"
#include "mdw_ap_tag.h"
#include "mdw_dmy.h"

static int mdw_ap_sw_init(struct mdw_device *mdev)
{
	struct mdw_rsc_tab *t = NULL;
	struct mdw_dinfo *d = NULL;
	unsigned int i = 0;
	int ret = 0;

	/* update device info */
	for (i = 0; i < MDW_DEV_MAX; i++) {
		t = mdw_rsc_get_tab(i);
		if (!t || mdev->dinfos[i])
			continue;

		/* setup mdev's info */
		d = kzalloc(sizeof(*d), GFP_KERNEL);
		if (!d)
			goto free_dinfo;
		d->num = t->dev_num;
		d->type = t->type;
		if (t->array[0]->dev) {
			memcpy(d->meta, t->array[0]->dev->meta_data,
				sizeof(d->meta));
		}

		mdev->dinfos[i] = d;
		bitmap_set(mdev->dev_mask, i, 1);
	}

	goto out;

free_dinfo:
	for (i = 0; i < MDW_DEV_MAX; i++) {
		if (mdev->dinfos[i] != NULL) {
			kfree(mdev->dinfos[i]);
			mdev->dinfos[i] = NULL;
		}
	}
	ret = -ENOMEM;
out:
	return ret;
}

static void mdw_ap_sw_deinit(struct mdw_device *mdev)
{
	unsigned int i = 0;

	for (i = 0; i < MDW_DEV_MAX; i++) {
		if (mdev->dinfos[i] != NULL) {
			kfree(mdev->dinfos[i]);
			mdev->dinfos[i] = NULL;
		}
	}
}

static int mdw_ap_late_init(struct mdw_device *mdev)
{
	int ret = 0;

	mdw_ap_tag_init();

	/* init resource mgr */
	ret = mdw_rsc_init();
	if (ret) {
		mdw_drv_err("rsc init fail\n");
		goto out;
	}

	/* init dmy driver */
	ret = mdw_dmy_init();
	if (ret) {
		mdw_drv_err("init dmy dev fail\n");
		goto rsc_deinit;
	}

	/* init mem infos */
	ret = mdw_rvs_get_vlm_property(
		&mdev->minfos[MDW_MEM_TYPE_VLM].device_va,
		&mdev->minfos[MDW_MEM_TYPE_VLM].dva_size);
	if (ret) {
		mdw_drv_warn("vlm wrong\n");
		mdev->minfos[MDW_MEM_TYPE_VLM].device_va = 0;
		mdev->minfos[MDW_MEM_TYPE_VLM].dva_size = 0;
	} else {
		bitmap_set(mdev->mem_mask, MDW_MEM_TYPE_VLM, 1);
	}

	mdev->inited = true;

	goto out;

rsc_deinit:
	mdw_rsc_deinit();
out:
	return ret;
}

static void mdw_ap_late_deinit(struct mdw_device *mdev)
{
	mdw_dmy_deinit();
	mdw_rsc_deinit();
	mdw_ap_tag_deinit();
}

static int mdw_ap_run_cmd(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	return mdw_ap_cmd_exec(c);
}

static int mdw_ap_set_power(struct mdw_device *mdev,
	uint32_t type, uint32_t idx, uint32_t boost)
{
	struct mdw_dev_info *d = NULL;

	d = mdw_rsc_get_dinfo(type, idx);
	if (!d)
		return -ENODEV;

	return d->pwr_on(d, boost, MDW_RSC_SET_PWR_TIMEOUT);
}

static int mdw_ap_ucmd(struct mdw_device *mdev,
	uint32_t type, void *vaddr, uint32_t size)
{
	struct mdw_dev_info *d = NULL;

	d = mdw_rsc_get_dinfo(type, 0);
	if (!d)
		return -ENODEV;

	return d->ucmd(d, (uint64_t)vaddr, 0, size);
}

static int mdw_ap_set_param(struct mdw_device *mdev,
	enum mdw_info_type type, uint32_t val)
{
	int ret = 0;

	switch (type) {
	case MDW_INFO_KLOG:
		g_mdw_klog = val;
		break;

	default:
		ret = -EINVAL;
		mdw_drv_warn("unknown type(%u)\n", type);
		break;
	}

	return ret;
}

static uint32_t mdw_ap_get_info(struct mdw_device *mdev,
	enum mdw_info_type type)
{
	struct mdw_queue *mq = NULL;
	uint32_t ret = 0;

	switch (type) {
	case MDW_INFO_KLOG:
		ret = g_mdw_klog;
		break;

	case MDW_INFO_NORMAL_TASK_DLA:
		mq = mdw_rsc_get_queue(APUSYS_DEVICE_MDLA);
		if (!mq)
			break;
		ret = mq->normal_task_num;
		break;

	case MDW_INFO_NORMAL_TASK_DSP:
		mq = mdw_rsc_get_queue(APUSYS_DEVICE_VPU);
		if (!mq)
			break;
		ret = mq->normal_task_num;
		break;

	case MDW_INFO_NORMAL_TASK_DMA:
		mq = mdw_rsc_get_queue(APUSYS_DEVICE_EDMA);
		if (!mq)
			break;
		ret = mq->normal_task_num;
		break;

	default:
		mdw_drv_warn("unknown type(%d)\n", type);
		break;
	}

	return ret;
}

static const struct mdw_dev_func mdw_ap_func = {
	.sw_init = mdw_ap_sw_init,
	.sw_deinit = mdw_ap_sw_deinit,
	.late_init = mdw_ap_late_init,
	.late_deinit = mdw_ap_late_deinit,
	.run_cmd = mdw_ap_run_cmd,
	.set_power = mdw_ap_set_power,
	.ucmd = mdw_ap_ucmd,
	.set_param = mdw_ap_set_param,
	.get_info = mdw_ap_get_info,
	.register_device = mdw_rsc_register_device,
	.unregister_device = mdw_rsc_unregister_device,
};

void mdw_ap_set_func(struct mdw_device *mdev)
{
	/* check mdw ap version support */
	if (mdev->mdw_ver != 1) {
		mdw_drv_err("invalid version(%u) for ap\n", mdev->mdw_ver);
		return;
	}

	mdev->dev_funcs = &mdw_ap_func;
	mdev->uapi_ver = 2;
}
