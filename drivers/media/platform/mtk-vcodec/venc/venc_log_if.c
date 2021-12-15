/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "../mtk_vcodec_drv.h"
#include "../venc_drv_base.h"
#include "../venc_ipi_msg.h"
#include "../venc_vcu_if.h"

static int venc_log_set_param(unsigned long handle,
	enum venc_set_param_type type,
	struct venc_enc_param *enc_prm)
{
	int ret = 0;

	if (enc_prm == NULL) {
		pr_info("%s, enc_prm is null", __func__);
		return -EINVAL;
	}

	switch (type) {
	case VENC_SET_PARAM_LOG:
		ret = vcu_set_log(enc_prm->log);
		break;
	default:
		pr_info("invalid set parameter type=%d\n", type);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static const struct venc_common_if venc_log_if = {
	.set_param = venc_log_set_param,
};

const struct venc_common_if *get_enc_log_if(void)
{
	return &venc_log_if;
}
