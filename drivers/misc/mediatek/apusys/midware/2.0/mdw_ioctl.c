// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/uaccess.h>

#include "mdw_ioctl.h"
#include "mdw_cmn.h"

long mdw_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int usize = 0;
	void *kdata = NULL;
	struct mdw_fpriv *mpriv = filp->private_data;

	/* allocate for user data */
	usize = _IOC_SIZE(cmd);
	kdata = kzalloc(usize, GFP_KERNEL);
	if (!kdata)
		return -ENOMEM;
	/* copy from user data */
	if (cmd & IOC_IN) {
		if (copy_from_user(kdata, (void __user *)arg, usize)) {
			ret = -EFAULT;
			goto out;
		}
	}

	switch (cmd) {
	case APU_MDW_IOCTL_HANDSHAKE:
		ret = mdw_hs_ioctl(mpriv, kdata);
		break;
	case APU_MDW_IOCTL_MEM:
		ret = mdw_mem_ioctl(mpriv, kdata);
		break;
	case APU_MDW_IOCTL_CMD:
		ret = mdw_cmd_ioctl(mpriv, kdata);
		break;
	case APU_MDW_IOCTL_UTIL:
		ret = mdw_util_ioctl(mpriv, kdata);
		break;
	default:
		ret = -EFAULT;
		goto out;
	}

	/* copy to user data */
	if (cmd & IOC_OUT) {
		if (copy_to_user((void __user *)arg, kdata, usize)) {
			ret = -EFAULT;
			goto out;
		}
	}

out:
	kfree(kdata);

	return ret;
}
