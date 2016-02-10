/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ipahal.h"
#include "ipahal_i.h"
#include "ipahal_reg_i.h"

struct ipahal_context *ipahal_ctx;

int ipahal_init(enum ipa_hw_type ipa_hw_type, void __iomem *base)
{
	int result;

	IPAHAL_DBG("Entry - IPA HW TYPE=%d base=%p\n",
		ipa_hw_type, base);

	ipahal_ctx = kzalloc(sizeof(*ipahal_ctx), GFP_KERNEL);
	if (!ipahal_ctx) {
		IPAHAL_ERR("kzalloc err for ipahal_ctx\n");
		result = -ENOMEM;
		goto bail_err_exit;
	}

	if (ipa_hw_type < IPA_HW_v3_0) {
		IPAHAL_ERR("ipahal supported on IPAv3 and later only\n");
		result = -EINVAL;
		goto bail_free_ctx;
	}

	if (!base) {
		IPAHAL_ERR("invalid memory io mapping addr\n");
		result = -EINVAL;
		goto bail_free_ctx;
	}

	ipahal_ctx->hw_type = ipa_hw_type;
	ipahal_ctx->base = base;

	if (ipahal_reg_init(ipa_hw_type)) {
		IPAHAL_ERR("failed to init ipahal reg\n");
		result = -EFAULT;
		goto bail_free_ctx;
	}

	return 0;

bail_free_ctx:
	kfree(ipahal_ctx);
	ipahal_ctx = NULL;
bail_err_exit:
	return result;
}

void ipahal_destroy(void)
{
	IPAHAL_DBG("Entry\n");

	kfree(ipahal_ctx);
	ipahal_ctx = NULL;
}
