// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/v4l2-controls.h>

#include "mtk-hxp-drv.h"
#include "slbc_ops.h"

#define V4L2_CID_IMGSYS_OFFSET	(0xC000)
#define V4L2_CID_IMGSYS_BASE    (V4L2_CID_USER_BASE + V4L2_CID_IMGSYS_OFFSET)
#define V4L2_CID_IMGSYS_APU_DC	(V4L2_CID_IMGSYS_BASE + 1)

#define HXP_CTRL_ID_SLB_BASE    (0x01)

struct ctrl_data {
	uint32_t id;
	uintptr_t value;
} __packed;

int hxp_ctrl_init(struct mtk_hxp *hxp_dev)
{
	struct hxp_ctrl *ctrl_info = &hxp_dev->ctrl_info;

	atomic_set(&(ctrl_info->have_slb), 0);

	return 0;
}

static int set_apu_dc(struct mtk_hxp *hxp_dev, int32_t value, size_t size)
{
	struct hxp_ctrl *ctrl_info = &hxp_dev->ctrl_info;

	struct slbc_data slb;
	struct ctrl_data ctrl;
	int ret;

	if (value) {
		if (atomic_inc_return(&(ctrl_info->have_slb)) == 1) {
			slb.uid = UID_SH_P2;
			slb.type = TP_BUFFER;
			ret = slbc_request(&slb);
			if (ret < 0) {
				dev_info(hxp_dev->dev, "%s: Failed to allocate SLB buffer",
					__func__);
				return -1;
			}

			dev_info(hxp_dev->dev, "%s: SLB buffer base(0x%x), size(%ld): %x",
				__func__, (uintptr_t)slb.paddr, slb.size);

			ctrl.id    = HXP_CTRL_ID_SLB_BASE;
			ctrl.value = ((slb.size << 32) |
				((uintptr_t)slb.paddr & 0x0FFFFFFFFULL));

			//return hxp_send_internal(hxp_dev,
			//	HXP_IMGSYS_SET_CONTROL_ID, &ctrl, sizeof(ctrl), 0, 0);
		}
	} else {
		if (atomic_dec_return(&(ctrl_info->have_slb)) == 0) {
			slb.uid  = UID_SH_P2;
			slb.type = TP_BUFFER;
			ret = slbc_release(&slb);
			if (ret < 0) {
				dev_info(hxp_dev->dev, "Failed to release SLB buffer");
				return -1;
			}

			ctrl.id    = HXP_CTRL_ID_SLB_BASE;
			ctrl.value = 0;

			//return hxp_send_internal(hxp_dev,
			//	HXP_IMGSYS_SET_CONTROL_ID, &ctrl, sizeof(ctrl), 0, 0);
		}
	}

	return 0;
}

int hxp_ctrl_set(struct mtk_hxp *hxp_dev,
	int32_t code, int32_t value, size_t size)
{
	int ret;

	switch (code) {
	case V4L2_CID_IMGSYS_APU_DC:
		ret = set_apu_dc(hxp_dev, value, size);
	default:
	  ret = -1;
	  pr_info("%s: non-supported ctrl code(%x)\n", __func__, code);
		break;
	}

	return ret;
}

int hxp_ctrl_reset(struct mtk_hxp *hxp_dev)
{
	struct hxp_ctrl *ctrl_info = &hxp_dev->ctrl_info;
	struct slbc_data slb;
	int ret;

	if (atomic_read(&(ctrl_info->have_slb)) > 0) {
		slb.uid  = UID_SH_P2;
		slb.type = TP_BUFFER;
		ret = slbc_release(&slb);
		if (ret < 0) {
			dev_info(hxp_dev->dev, "failed to release slb buffer");
			return -1;
		}
	}

	return 0;
}

int hxp_ctrl_uninit(struct mtk_hxp *hxp_dev)
{
	struct hxp_ctrl *ctrl_info = &hxp_dev->ctrl_info;
	struct slbc_data slb;
	int ret;

	if (atomic_read(&(ctrl_info->have_slb)) > 0) {
		slb.uid  = UID_SH_P2;
		slb.type = TP_BUFFER;
		ret = slbc_release(&slb);
		if (ret < 0) {
			dev_info(hxp_dev->dev, "failed to release slb buffer");
			return -1;
		}
	}

	return 0;
}
