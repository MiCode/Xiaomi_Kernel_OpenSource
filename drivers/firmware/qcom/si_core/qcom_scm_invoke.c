// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/firmware/qcom/qcom_scm.h>

#include "si_core.h"

/* 6 Sec. retry seems reasonable!? */
#define SCM_EBUSY_WAIT_MS 30
#define SCM_EBUSY_MAX_RETRY 200

int si_object_invoke_ctx_invoke(struct si_object_invoke_ctx *oic,
	int *result, u64 *response_type, unsigned int *data)
{
	int ret, i = 0;

	/* TODO. buffers always coherent!? */

	do {
		/* Direct invocation of callback!? */
		if (!(oic->flags & OIC_FLAG_BUSY)) {
			ret = qcom_scm_invoke_smc(oic->in.paddr,
				oic->in.msg.size,
				oic->out.paddr,
				oic->out.msg.size,
				result,
				response_type,
				data);

		} else {
			ret = qcom_scm_invoke_callback_response(oic->out.paddr,
				oic->out.msg.size,
				result,
				response_type,
				data);
		}

		if (ret != -EBUSY)
			break;

		msleep(SCM_EBUSY_WAIT_MS);

	} while (++i < SCM_EBUSY_MAX_RETRY);

	if (ret)
		pr_err("QTEE returned with %d!\n", ret);

	return ret;
}
