// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_ioctl.h"
#include "mdw_cmn.h"

int mdw_util_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_util_args *args = (union mdw_util_args *)data;
	struct mdw_util_in *in = (struct mdw_util_in *)args;
	struct mdw_device *mdev = mpriv->mdev;
	void *mem_ucmd = NULL;
	int ret = 0;

	mdw_flw_debug("s(0x%llx) op::%d\n", (uint64_t)mpriv, args->in.op);

	switch (args->in.op) {
	case MDW_UTIL_IOCTL_SETPOWER:
		ret = mdev->dev_funcs->set_power(mdev, in->power.dev_type,
			in->power.core_idx, in->power.boost);
		break;

	case MDW_UTIL_IOCTL_UCMD:
		if (!in->ucmd.size || !in->ucmd.handle) {
			mdw_drv_err("invalid ucmd(%u/0x%llx) param\n",
				in->ucmd.size, in->ucmd.handle);
			ret = -EINVAL;
			break;
		}

		mem_ucmd = vzalloc(args->in.ucmd.size);
		if (!mem_ucmd) {
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(mem_ucmd,
			(void __user *)in->ucmd.handle,
			in->ucmd.size)) {
			ret = -EFAULT;
			goto free_ucmd;
		}
		ret = mdev->dev_funcs->ucmd(mdev, in->ucmd.dev_type,
			mem_ucmd, in->ucmd.size);
		if (ret) {
			mdw_drv_err("dev(%d) ucmd fail\n", in->ucmd.dev_type);
			goto free_ucmd;
		}

		if (copy_to_user((void __user *)in->ucmd.handle,
				mem_ucmd, in->ucmd.size))
			ret = -EFAULT;

free_ucmd:
		vfree(mem_ucmd);
		break;

	default:
		mdw_drv_err("invalid util op code(%d)\n", args->in.op);
		ret = -EINVAL;
		break;
	}

	return ret;
}
