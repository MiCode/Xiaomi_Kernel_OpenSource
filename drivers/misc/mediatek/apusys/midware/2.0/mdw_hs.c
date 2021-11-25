// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_ioctl.h"
#include "mdw_cmn.h"

int mdw_hs_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_hs_args *args = (union mdw_hs_args *)data;
	struct mdw_device *mdev = mpriv->mdev;
	unsigned int type = 0;
	int ret = 0;

	mdw_flw_debug("s(0x%llx) op:%d\n", (uint64_t)mpriv, args->in.op);

	switch (args->in.op) {
	case MDW_HS_IOCTL_OP_BASIC:
		/* assign basic infos */
		memset(args, 0, sizeof(*args));
		args->out.basic.version = mdev->uapi_ver;
		memcpy(&args->out.basic.dev_bitmask,
			mdev->dev_mask, sizeof(mdev->dev_mask));
		memcpy(&args->out.basic.mem_bitmask,
			mdev->mem_mask, sizeof(mdev->mem_mask));
		args->out.basic.meta_size = MDW_DEV_META_SIZE;
		mdw_flw_debug("version(%u) dev mask(0x%llx) mem mask(0x%llx)\n",
			args->out.basic.version,
			args->out.basic.dev_bitmask,
			args->out.basic.mem_bitmask);
		break;

	case MDW_HS_IOCTL_OP_DEV:
		/* check type valid */
		type = args->in.dev.type;
		if (type >= MDW_DEV_MAX) {
			ret = -EINVAL;
			break;
		}

		/* assign dev infos */
		memset(args, 0, sizeof(*args));
		args->out.dev.type = type;
		if (mdev->dinfos[type] == NULL) {
			ret = -EINVAL;
			mdw_drv_err("dev type(%u) not support\n", type);
			break;
		}

		args->out.dev.num = mdev->dinfos[type]->num;
		memcpy(args->out.dev.meta, mdev->dinfos[type]->meta,
			sizeof(args->out.dev.meta));
		mdw_flw_debug("dev(%u) num(%u) meta(%s)\n",
			args->out.dev.type, args->out.dev.num,
			mdev->dinfos[type]->meta);
		break;

	case MDW_HS_IOCTL_OP_MEM:
		/* check type valid */
		type = args->in.mem.type;
		if (type >= MDW_MEM_TYPE_MAX) {
			mdw_drv_err("unknown mem type(%u)\n", type);
			ret = -EINVAL;
			break;
		}

		/* assign mem infos */
		memset(args, 0, sizeof(*args));
		args->out.mem.type = type;

		args->out.mem.start = mdev->minfos[type].device_va;
		args->out.mem.size = mdev->minfos[type].dva_size;
		mdw_flw_debug("mem(%u) start(0x%llx) size(0x%x)\n",
			args->out.mem.type,
			args->out.mem.start,
			args->out.mem.size);
		break;

	default:
		mdw_drv_err("invalid handshake op code(%d)\n", args->in.op);
		ret = -EINVAL;
		break;
	}

	return ret;
}
