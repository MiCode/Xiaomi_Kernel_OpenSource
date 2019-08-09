/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/mutex.h>
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
#include <memory_ssmr.h>
#endif

#include "private/tmem_utils.h"
#include "private/tmem_error.h"
#include "private/tmem_device.h"

static int tmem_ssmr_get(u64 *pa, u32 *size, u32 feat, void *priv)
{
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
	phys_addr_t ssmr_pa;
	unsigned long ssmr_size;

	UNUSED(priv);

	if (ssmr_offline(&ssmr_pa, &ssmr_size, true, feat)) {
		pr_err("ssmr offline falied (feat:%d)!\n", feat);
		return TMEM_SSMR_OFFLINE_FAILED;
	}

	*pa = (u64)ssmr_pa;
	*size = (u32)ssmr_size;
	if (INVALD_ADDR(*pa) || INVALD_SIZE(*size)) {
		pr_err("ssmr pa is invalid (0x%llx, 0x%x)\n", *pa, *size);
		return TMEM_INVALID_ADDR_OR_SIZE;
	}

	pr_debug("ssmr offline passed! feat:%d, pa: 0x%llx, sz: 0x%x\n", feat,
		 *pa, *size);
	return TMEM_OK;
#else
	pr_err("%s:%d operation is not implemented yet!\n", __func__, __LINE__);
	return TMEM_OPERATION_NOT_IMPLEMENTED;
#endif
}

static int tmem_ssmr_put(u32 feat, void *priv)
{
	UNUSED(priv);
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
	if (ssmr_online(feat)) {
		pr_err("ssmr online failed (feat:%d)!\n", feat);
		return TMEM_SSMR_ONLINE_FAILED;
	}

	pr_debug("ssmr online passed!\n");
	return TMEM_OK;
#else
	pr_err("%s:%d operation is not implemented yet!\n", __func__, __LINE__);
	return TMEM_OPERATION_NOT_IMPLEMENTED;
#endif
}

static struct ssmr_operations tmem_ssmr_ops = {
	.offline = tmem_ssmr_get,
	.online = tmem_ssmr_put,
};

void get_ssmr_ops(struct ssmr_operations **ops)
{
	*ops = &tmem_ssmr_ops;
}
