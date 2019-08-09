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
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include "private/mld_helper.h"
#include "private/tmem_device.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "tee_impl/tee_priv.h"
#include "tee_impl/tee_common.h"

#define TEE_CMD_LOCK() mutex_lock(&tee_lock)
#define TEE_CMD_UNLOCK() mutex_unlock(&tee_lock)

static DEFINE_MUTEX(tee_lock);
static struct trusted_driver_operations *tee_ops;
static void *tee_session_data;

static inline int
tee_directly_invoke_cmd_locked(struct trusted_driver_cmd_params *invoke_params)
{
	int ret = TMEM_OK;

	if (unlikely(INVALID(invoke_params)))
		return TMEM_PARAMETER_ERROR;

	if (unlikely(INVALID(tee_ops)))
		get_tee_peer_ops(&tee_ops);

	if (tee_ops->session_open(&tee_session_data, NULL)) {
		pr_err("%s:%d tee open session failed!\n", __func__, __LINE__);
		return TMEM_TEE_CREATE_SESSION_FAILED;
	}

	ret = tee_ops->invoke_cmd(invoke_params, tee_session_data, NULL);

	if (tee_ops->session_close(tee_session_data, NULL))
		pr_err("%s:%d tee close session failed!\n", __func__, __LINE__);

	return ret;
}

int tee_directly_invoke_cmd(struct trusted_driver_cmd_params *invoke_params)
{
	int ret = TMEM_OK;

	TEE_CMD_LOCK();
	ret = tee_directly_invoke_cmd_locked(invoke_params);
	TEE_CMD_UNLOCK();

	return ret;
}
