// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "apusys_drv.h"
#include "mdw_cmn.h"

int mdw_hs_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_hs_args *args = (union mdw_hs_args *)data;
	struct mdw_device *mdev = mpriv->mdev;
	unsigned int type = 0;
	int ret = 0;

	mdw_flw_debug("op:%d\n", args->in.op);

	switch (args->in.op) {
	case MDW_HS_IOCTL_OP_BASIC:
		memset(args, 0, sizeof(*args));
		args->out.basic.version = mdev->version;
		bitmap_to_arr32((uint32_t *)&args->out.basic.dev_bitmask,
			mdev->dev_mask, MDW_DEV_MAX);
		args->out.basic.meta_size = MDW_DEV_META_SIZE;
		args->out.basic.vlm_start = mdev->vlm_start;
		args->out.basic.vlm_size = mdev->vlm_size;
		mdw_flw_debug("version(%u) dev mask(0x%llx)\n",
			args->out.basic.version, args->out.basic.dev_bitmask);
		break;

	case MDW_HS_IOCTL_OP_DEV:
		type = args->in.dev.type;
		if (type >= MDW_DEV_MAX) {
			ret = -EINVAL;
			break;
		}

		if (mdev->dinfos[type] == NULL) {
			ret = -EINVAL;
			break;
		}

		memset(args, 0, sizeof(*args));
		args->out.dev.type = type;
		args->out.dev.num = mdev->dinfos[type]->num;
		memcpy(args->out.dev.meta, mdev->dinfos[type]->meta,
			sizeof(args->out.dev.meta));
		mdw_flw_debug("dev(%u) num(%u) meta(%s)\n",
			args->out.dev.type, args->out.dev.num,
			mdev->dinfos[type]->meta);
		break;

	default:
		mdw_drv_err("invalid handshake op code(%d)\n", args->in.op);
		ret = -EINVAL;
		break;
	}

	return ret;
}
