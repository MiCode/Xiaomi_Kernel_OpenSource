/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "vdec_drv_base.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_drv.h"
#include "vdec_vcu_if.h"

static int vdec_log_set_param(unsigned long h_vdec,
	enum vdec_set_param_type type, void *in)
{	int ret = 0;

	if (in == NULL) {
		pr_info("%s, in is null", __func__);
		return -EINVAL;
	}

	switch (type) {
	case SET_PARAM_DEC_LOG:
		ret = vcu_set_log((char *) in);
		break;
	default:
		pr_info("invalid set parameter type=%d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct vdec_common_if vdec_log_if = {
	.set_param = vdec_log_set_param,
};

const struct vdec_common_if *get_dec_log_if(void)
{
	return &vdec_log_if;
}
