// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_cmn.h"
#include "mdw_rv.h"
#include "mdw_rv_tag.h"

static int mdw_rv_sw_init(struct mdw_device *mdev)
{
	int ret = 0, i = 0;
	struct mdw_rv_dev *mrdev = (struct mdw_rv_dev *)mdev->dev_specific;
	struct mdw_dinfo *d = NULL;

	/* update device infos */
	for (i = 0; i < MDW_DEV_MAX; i++) {
		if (!test_bit(i, mrdev->dev_mask) || mdev->dinfos[i])
			continue;

		/* setup mdev's info */
		d = kzalloc(sizeof(*d), GFP_KERNEL);
		if (!d)
			goto free_dinfo;

		d->num = mrdev->dev_num[i];
		d->type = i;

		/* meta data */
		memcpy(d->meta, &mrdev->meta_data[i][0], sizeof(d->meta));
		mdw_drv_debug("dev(%u) support (%u)core\n", d->type, d->num);

		mdev->dinfos[i] = d;
		bitmap_set(mdev->dev_mask, i, 1);
	}

	/* update mem infos */
	mrdev = (struct mdw_rv_dev *)mdev->dev_specific;
	memcpy(mdev->mem_mask, mrdev->mem_mask, sizeof(mdev->mem_mask));
	memcpy(mdev->minfos, mrdev->minfos, sizeof(mdev->minfos));

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
	return 0;
}

static void mdw_rv_sw_deinit(struct mdw_device *mdev)
{
	unsigned int i = 0;

	for (i = 0; i < MDW_DEV_MAX; i++) {
		if (mdev->dinfos[i] != NULL) {
			kfree(mdev->dinfos[i]);
			mdev->dinfos[i] = NULL;
		}
	}
}

static int mdw_rv_late_init(struct mdw_device *mdev)
{
	int ret = 0;

	mdw_rv_tag_init();

	/* init rv device */
	ret = mdw_rv_dev_init(mdev);
	if (ret || !mdev->dev_specific) {
		mdw_drv_err("init mdw rvdev fail(%d)\n", ret);
		goto dev_deinit;
	}

	goto out;

dev_deinit:
	mdw_rv_dev_deinit(mdev);
out:
	return ret;
}

static void mdw_rv_late_deinit(struct mdw_device *mdev)
{
	mdw_rv_dev_deinit(mdev);
	mdw_rv_tag_deinit();
}

static int mdw_rv_run_cmd(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	return mdw_rv_dev_run_cmd(mpriv, c);
}

static int mdw_rv_set_power(struct mdw_device *mdev,
	uint32_t type, uint32_t idx, uint32_t boost)
{
	return -EINVAL;
}

static int mdw_rv_ucmd(struct mdw_device *mdev,
	uint32_t type, void *vaddr, uint32_t size)
{
	return -EINVAL;
}

static int mdw_rv_set_param(struct mdw_device *mdev,
	enum mdw_info_type type, uint32_t val)
{
	struct mdw_rv_dev *mrdev = (struct mdw_rv_dev *)mdev->dev_specific;

	return mdw_rv_dev_set_param(mrdev, type, val);
}

static uint32_t mdw_rv_get_info(struct mdw_device *mdev,
	enum mdw_info_type type)
{
	struct mdw_rv_dev *mrdev = (struct mdw_rv_dev *)mdev->dev_specific;
	int val = 0;

	mdw_rv_dev_get_param(mrdev, type, &val);
	return val;
}

static int mdw_rv_register_device(struct apusys_device *adev)
{
	return 0;
}

static int mdw_rv_unregister_device(struct apusys_device *adev)
{
	return 0;
}

static const struct mdw_dev_func mdw_rv_func = {
	.sw_init = mdw_rv_sw_init,
	.sw_deinit = mdw_rv_sw_deinit,
	.late_init = mdw_rv_late_init,
	.late_deinit = mdw_rv_late_deinit,
	.run_cmd = mdw_rv_run_cmd,
	.set_power = mdw_rv_set_power,
	.ucmd = mdw_rv_ucmd,
	.set_param = mdw_rv_set_param,
	.get_info = mdw_rv_get_info,
	.register_device = mdw_rv_register_device,
	.unregister_device = mdw_rv_unregister_device,
};

void mdw_rv_set_func(struct mdw_device *mdev)
{
	mdev->dev_funcs = &mdw_rv_func;
	mdev->uapi_ver = 2;
}
