/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2011-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __QCOM_CRYPTO_DEVICE__H
#define __QCOM_CRYPTO_DEVICE__H

#include <linux/types.h>

struct msm_ce_hw_support {
	uint32_t ce_shared;
	uint32_t shared_ce_resource;
	uint32_t hw_key_support;
	uint32_t sha_hmac;
};

#endif /* __QCOM_CRYPTO_DEVICE__H */
