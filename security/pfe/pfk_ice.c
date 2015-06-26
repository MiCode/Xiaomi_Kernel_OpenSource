/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
	TZ_SYSCALL_CREATE_PARAM_ID_3( \
			TZ_SYSCALL_PARAM_TYPE_VAL, \
			TZ_SYSCALL_PARAM_TYPE_BUF_RW, TZ_SYSCALL_PARAM_TYPE_VAL)

#define TZ_ES_INVALIDATE_ICE_KEY_PARAM_ID \
	TZ_SYSCALL_CREATE_PARAM_ID_1( \
	TZ_SYSCALL_PARAM_TYPE_VAL)

#define ICE_KEY_SIZE 32


uint8_t ice_key[ICE_KEY_SIZE];

int qti_pfk_ice_set_key(uint32_t index, uint8_t *key)
{
	struct scm_desc desc = {0};
	int ret;
	char *tzbuf = (char *)ice_key;

	uint32_t smc_id = 0;
	u32 tzbuflen = sizeof(ice_key);

	if (index < MIN_ICE_KEY_INDEX || index > MAX_ICE_KEY_INDEX)
		return -EINVAL;

	if (!tzbuf)
		return -ENOMEM;

	memset(tzbuf, 0, tzbuflen);
	memcpy(ice_key, key, ICE_KEY_SIZE);

	dmac_flush_range(tzbuf, tzbuf + tzbuflen);

	smc_id = TZ_ES_SET_ICE_KEY_ID;
	pr_debug(" %s , smc_id = 0x%x\n", __func__, smc_id);

	desc.arginfo = TZ_ES_SET_ICE_KEY_PARAM_ID;
	desc.args[0] = index;
	desc.args[1] = virt_to_phys(tzbuf);
	desc.args[2] = tzbuflen;

	ret = scm_call2_atomic(smc_id, &desc);
	pr_debug(" %s , ret = %d\n", __func__, ret);
	if (ret)
		pr_err("%s: Error: 0x%x\n", __func__, ret);

	return ret;
}


int qti_pfk_ice_invalidate_key(uint32_t index)
{
	struct scm_desc desc = {0};
	int ret;

	uint32_t smc_id = 0;

	if (index < MIN_ICE_KEY_INDEX || index > MAX_ICE_KEY_INDEX)
		return -EINVAL;

	smc_id = TZ_ES_INVALIDATE_ICE_KEY_ID;
	pr_debug(" %s , smc_id = 0x%x\n", __func__, smc_id);

	desc.arginfo = TZ_ES_INVALIDATE_ICE_KEY_PARAM_ID;
	desc.args[0] = index;

	ret = scm_call2_atomic(smc_id, &desc);

	pr_debug(" %s , ret = %d\n", __func__, ret);
	if (ret)
		pr_err("%s: Error: 0x%x\n", __func__, ret);

	return ret;

}
