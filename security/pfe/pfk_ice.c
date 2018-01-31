/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/async.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <soc/qcom/scm.h>
#include <linux/device-mapper.h>
#include <soc/qcom/qseecomi.h>
#include <crypto/ice.h>
#include "pfk_ice.h"


/**********************************/
/** global definitions		 **/
/**********************************/

#define TZ_ES_SET_ICE_KEY 0x2
#define TZ_ES_INVALIDATE_ICE_KEY 0x3

/* index 0 and 1 is reserved for FDE */
#define MIN_ICE_KEY_INDEX 2

#define MAX_ICE_KEY_INDEX 31


#define TZ_ES_SET_ICE_KEY_ID \
	TZ_SYSCALL_CREATE_SMC_ID(TZ_OWNER_SIP, TZ_SVC_ES, TZ_ES_SET_ICE_KEY)


#define TZ_ES_INVALIDATE_ICE_KEY_ID \
		TZ_SYSCALL_CREATE_SMC_ID(TZ_OWNER_SIP, \
			TZ_SVC_ES, TZ_ES_INVALIDATE_ICE_KEY)


#define TZ_ES_SET_ICE_KEY_PARAM_ID \
	TZ_SYSCALL_CREATE_PARAM_ID_5( \
		TZ_SYSCALL_PARAM_TYPE_VAL, \
		TZ_SYSCALL_PARAM_TYPE_BUF_RW, TZ_SYSCALL_PARAM_TYPE_VAL, \
		TZ_SYSCALL_PARAM_TYPE_BUF_RW, TZ_SYSCALL_PARAM_TYPE_VAL)

#define TZ_ES_INVALIDATE_ICE_KEY_PARAM_ID \
	TZ_SYSCALL_CREATE_PARAM_ID_1( \
	TZ_SYSCALL_PARAM_TYPE_VAL)

#define ICE_KEY_SIZE 32
#define ICE_SALT_SIZE 32

static uint8_t ice_key[ICE_KEY_SIZE];
static uint8_t ice_salt[ICE_KEY_SIZE];

int qti_pfk_ice_set_key(uint32_t index, uint8_t *key, uint8_t *salt,
			char *storage_type)
{
	struct scm_desc desc = {0};
	int ret, ret1;
	char *tzbuf_key = (char *)ice_key;
	char *tzbuf_salt = (char *)ice_salt;
	char *s_type = storage_type;

	uint32_t smc_id = 0;
	u32 tzbuflen_key = sizeof(ice_key);
	u32 tzbuflen_salt = sizeof(ice_salt);

	if (index < MIN_ICE_KEY_INDEX || index > MAX_ICE_KEY_INDEX) {
		pr_err("%s Invalid index %d\n", __func__, index);
		return -EINVAL;
	}
	if (!key || !salt) {
		pr_err("%s Invalid key/salt\n", __func__);
		return -EINVAL;
	}

	if (!tzbuf_key || !tzbuf_salt) {
		pr_err("%s No Memory\n", __func__);
		return -ENOMEM;
	}

	if (s_type == NULL) {
		pr_err("%s Invalid Storage type\n", __func__);
		return -EINVAL;
	}

	memset(tzbuf_key, 0, tzbuflen_key);
	memset(tzbuf_salt, 0, tzbuflen_salt);

	memcpy(ice_key, key, tzbuflen_key);
	memcpy(ice_salt, salt, tzbuflen_salt);

	dmac_flush_range(tzbuf_key, tzbuf_key + tzbuflen_key);
	dmac_flush_range(tzbuf_salt, tzbuf_salt + tzbuflen_salt);

	smc_id = TZ_ES_SET_ICE_KEY_ID;

	desc.arginfo = TZ_ES_SET_ICE_KEY_PARAM_ID;
	desc.args[0] = index;
	desc.args[1] = virt_to_phys(tzbuf_key);
	desc.args[2] = tzbuflen_key;
	desc.args[3] = virt_to_phys(tzbuf_salt);
	desc.args[4] = tzbuflen_salt;

	ret = qcom_ice_setup_ice_hw((const char *)s_type, true);

	if (ret) {
		pr_err("%s: could not enable clocks: %d\n", __func__, ret);
		goto out;
	}

	ret = scm_call2(smc_id, &desc);

	if (ret) {
		pr_err("%s: Set Key Error: %d\n", __func__, ret);
		if (ret == -EBUSY) {
			if (qcom_ice_setup_ice_hw((const char *)s_type, false))
				pr_err("%s: clock disable failed\n", __func__);
			goto out;
		}
		/*Try to invalidate the key to keep ICE in proper state*/
		smc_id = TZ_ES_INVALIDATE_ICE_KEY_ID;
		desc.arginfo = TZ_ES_INVALIDATE_ICE_KEY_PARAM_ID;
		desc.args[0] = index;
		ret1 = scm_call2(smc_id, &desc);
		if (ret1)
			pr_err("%s: Invalidate Key Error: %d\n", __func__,
					ret1);
	}
	ret = qcom_ice_setup_ice_hw((const char *)s_type, false);

out:
	return ret;
}

int qti_pfk_ice_invalidate_key(uint32_t index, char *storage_type)
{
	struct scm_desc desc = {0};
	int ret;

	uint32_t smc_id = 0;

	if (index < MIN_ICE_KEY_INDEX || index > MAX_ICE_KEY_INDEX) {
		pr_err("%s Invalid index %d\n", __func__, index);
		return -EINVAL;
	}

	if (storage_type == NULL) {
		pr_err("%s Invalid Storage type\n", __func__);
		return -EINVAL;
	}

	smc_id = TZ_ES_INVALIDATE_ICE_KEY_ID;

	desc.arginfo = TZ_ES_INVALIDATE_ICE_KEY_PARAM_ID;
	desc.args[0] = index;

	ret = qcom_ice_setup_ice_hw((const char *)storage_type, true);

	if (ret) {
		pr_err("%s: could not enable clocks: 0x%x\n", __func__, ret);
		return ret;
	}

	ret = scm_call2(smc_id, &desc);

	if (ret) {
		pr_err("%s: Error: 0x%x\n", __func__, ret);
		if (qcom_ice_setup_ice_hw((const char *)storage_type, false))
			pr_err("%s: could not disable clocks\n", __func__);
	} else {
		ret = qcom_ice_setup_ice_hw((const char *)storage_type, false);
	}

	return ret;
}
