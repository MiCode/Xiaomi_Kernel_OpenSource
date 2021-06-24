// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_cmn.h"
#include "mdw_rv.h"

static int mdw_rv_sw_init(struct mdw_device *mdev)
{
	int ret = 0, i = 0;
	struct mdw_rv_dev *rdev = NULL;
	struct mdw_dinfo *d = NULL;

	ret = mdw_rv_dev_handshake();
	if (ret)
		return ret;

	rdev = mdw_rv_dev_get();
	bitmap_from_arr32(mdev->dev_mask,
		(const uint32_t *)&rdev->dev_bitmask, MDW_DEV_MAX);

	/* update device info */
	for (i = 0; i < MDW_DEV_MAX; i++) {
		if (!(rdev->dev_bitmask & (1ULL << i) || mdev->dinfos[i]))
			continue;

		/* setup mdev's info */
		d = vzalloc(sizeof(*d));
		if (!d)
			goto free_dinfo;

		d->num = rdev->dev_num[i];
		d->type = i;
		memcpy(d->meta, &rdev->meta_data[i][0], sizeof(d->meta));
		mdw_drv_debug("dev(%u) support (%u)core\n", d->type, d->num);

		/* TODO meta data */
		mdev->dinfos[i] = d;
		bitmap_set(mdev->dev_mask, i, 1);
	}

	goto out;

free_dinfo:
	for (i = 0; i < MDW_DEV_MAX; i++) {
		if (mdev->dinfos[i] != NULL)
			vfree(mdev->dinfos[i]);
	}
	ret = -ENOMEM;
out:
	return 0;
}

static void mdw_rv_sw_deinit(struct mdw_device *mdev)
{


}

static int mdw_rv_late_init(struct mdw_device *mdev)
{
	/* TODO */
	mdev->vlm_start = 0x1D800000;
	mdev->vlm_size = 0x100000;

	return mdw_rv_dev_init(mdev);
}

static void mdw_rv_late_deinit(struct mdw_device *mdev)
{
	mdw_rv_dev_deinit(mdev);
}

static int mdw_rv_run_cmd(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	return mdw_rv_cmd_exec(mpriv, c);
}

static int mdw_rv_set_power(uint32_t type, uint32_t idx, uint32_t boost)
{
	return -EINVAL;
}

static int mdw_rv_ucmd(uint32_t type, void *vaddr, uint32_t size)
{
	return -EINVAL;
}

static int mdw_rv_lock(void)
{
	return mdw_rv_dev_lock();
}

static int mdw_rv_unlock(void)
{
	return mdw_rv_dev_unlock();
}

static int mdw_rv_set_param(uint32_t idx, uint32_t val)
{
	return mdw_rv_dev_set_param(idx, val);
}

static uint32_t mdw_rv_get_param(uint32_t idx)
{
	return mdw_rv_dev_get_param(idx);
}

static const struct mdw_dev_func mdw_rv_func = {
	.sw_init = mdw_rv_sw_init,
	.sw_deinit = mdw_rv_sw_deinit,
	.late_init = mdw_rv_late_init,
	.late_deinit = mdw_rv_late_deinit,
	.run_cmd = mdw_rv_run_cmd,
	.set_power = mdw_rv_set_power,
	.ucmd = mdw_rv_ucmd,
	.lock = mdw_rv_lock,
	.unlock = mdw_rv_unlock,
	.set_param = mdw_rv_set_param,
	.get_param = mdw_rv_get_param,
};

void mdw_rv_set_func(struct mdw_device *mdev)
{
	mdev->dev_funcs = &mdw_rv_func;
}
