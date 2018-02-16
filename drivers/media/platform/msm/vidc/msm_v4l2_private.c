/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_v4l2_private.h"

static int convert_from_user(struct msm_vidc_arg *kp, unsigned long arg)
{
	int rc = 0;
	struct msm_vidc_arg __user *up = compat_ptr(arg);

	if (get_user(kp->type, &up->type))
		return -EFAULT;

	switch (kp->type) {
	default:
		dprintk(VIDC_ERR, "%s: unknown cmd type 0x%x\n",
			__func__, kp->type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int convert_to_user(struct msm_vidc_arg *kp, unsigned long arg)
{
	int rc = 0;
	struct msm_vidc_arg __user *up = compat_ptr(arg);

	if (put_user(kp->type, &up->type))
		return -EFAULT;

	switch (kp->type) {
	default:
		dprintk(VIDC_ERR, "%s: unknown cmd type 0x%x\n",
			__func__, kp->type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

long msm_v4l2_private(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc;
	struct msm_vidc_inst *inst;
	struct msm_vidc_arg karg;

	if (!filp || !filp->private_data) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	inst = container_of(filp->private_data, struct msm_vidc_inst,
			event_handler);
	memset(&karg, 0, sizeof(struct msm_vidc_arg));

	/*
	 * the arg points to user space memory and needs
	 * to be converted to kernel space before using it.
	 * Check do_video_ioctl() for more details.
	 */
	if (convert_from_user(&karg, arg))
		return -EFAULT;

	rc = msm_vidc_private((void *)inst, cmd, &karg);
	if (rc) {
		dprintk(VIDC_ERR, "%s: failed cmd type %x\n",
			__func__, karg.type);
		return -EINVAL;
	}

	if (convert_to_user(&karg, arg))
		return -EFAULT;

	return rc;
}
