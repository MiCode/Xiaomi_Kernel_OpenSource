// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/bitmap.h>
#include "mdw_cmn.h"
#include "mdw_ap.h"

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
		d = vzalloc(sizeof(*d));
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
		if (mdev->dinfos[i] != NULL)
			vfree(&mdev->dinfos[i]);
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
			vfree(&mdev->dinfos[i]);
			mdev->dinfos[i] = NULL;
		}
	}
}

static int mdw_ap_late_init(struct mdw_device *mdev)
{
	mdw_rvs_get_vlm_property(&mdev->vlm_start, &mdev->vlm_size);

	return mdw_rsc_init();
}

static void mdw_ap_late_deinit(struct mdw_device *mdev)
{
	mdw_rsc_deinit();
}

static int mdw_ap_run_cmd(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	return mdw_ap_cmd_exec(c);
}

static int mdw_ap_set_power(uint32_t type, uint32_t idx, uint32_t boost)
{
	struct mdw_dev_info *d = NULL;

	d = mdw_rsc_get_dinfo(type, idx);
	if (!d)
		return -ENODEV;

	return d->pwr_on(d, boost, MDW_RSC_SET_PWR_TIMEOUT);
}

static int mdw_ap_ucmd(uint32_t type, void *vaddr, uint32_t size)
{
	struct mdw_dev_info *d = NULL;

	d = mdw_rsc_get_dinfo(type, 0);
	if (!d)
		return -ENODEV;

	return d->ucmd(d, (uint64_t)vaddr, 0, size);
}

static int mdw_ap_lock(void)
{
	mdw_drv_warn("not support\n");
	return -EINVAL;
}

static int mdw_ap_unlock(void)
{
	mdw_drv_warn("not support\n");
	return -EINVAL;
}

static int mdw_ap_set_param(uint32_t idx, uint32_t val)
{
	mdw_drv_warn("not support\n");
	return -EINVAL;
}

uint32_t mdw_ap_get_param(uint32_t idx)
{
	mdw_drv_warn("not support\n");
	return 0;
}

static const struct mdw_dev_func mdw_ap_func = {
	.sw_init = mdw_ap_sw_init,
	.sw_deinit = mdw_ap_sw_deinit,
	.late_init = mdw_ap_late_init,
	.late_deinit = mdw_ap_late_deinit,
	.run_cmd = mdw_ap_run_cmd,
	.set_power = mdw_ap_set_power,
	.ucmd = mdw_ap_ucmd,
	.lock = mdw_ap_lock,
	.unlock = mdw_ap_unlock,
	.set_param = mdw_ap_set_param,
	.get_param = mdw_ap_get_param,
};

void mdw_ap_set_func(struct mdw_device *mdev)
{
	mdev->dev_funcs = &mdw_ap_func;
}
