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
 *
 */
#ifndef __SOC_QCOM_RPMH_H__
#define __SOC_QCOM_RPMH_H__

#include <soc/qcom/tcs.h>
#include <linux/platform_device.h>

struct rpmh_client;

#ifdef CONFIG_QTI_RPMH_API
int rpmh_write_single(struct rpmh_client *rc, enum rpmh_state state,
			u32 addr, u32 data);

int rpmh_write_single_async(struct rpmh_client *rc,
			enum rpmh_state state, u32 addr, u32 data);

int rpmh_write(struct rpmh_client *rc, enum rpmh_state state,
			struct tcs_cmd *cmd, int n);

int rpmh_write_async(struct rpmh_client *rc, enum rpmh_state state,
			struct tcs_cmd *cmd, int n);

int rpmh_write_passthru(struct rpmh_client *rc, enum rpmh_state state,
			struct tcs_cmd *cmd, int *n);

int rpmh_write_control(struct rpmh_client *rc, struct tcs_cmd *cmd, int n);

int rpmh_invalidate(struct rpmh_client *rc);

int rpmh_flush(struct rpmh_client *rc);

int rpmh_read(struct rpmh_client *rc, u32 addr, u32 *resp);

struct rpmh_client *rpmh_get_byname(struct platform_device *pdev,
			const char *name);

struct rpmh_client *rpmh_get_byindex(struct platform_device *pdev,
			int index);

void rpmh_release(struct rpmh_client *rc);
#else
static inline int rpmh_write_single(struct rpmh_client *rc,
			enum rpmh_state state, u32 addr, u32 data)
{ return -ENODEV; }

static inline int rpmh_write_single_async(struct rpmh_client *rc,
			enum rpmh_state state, u32 addr, u32 data)
{ return -ENODEV; }

static inline int rpmh_write(struct rpmh_client *rc, enum rpmh_state state,
			struct tcs_cmd *cmd, int n)
{ return -ENODEV; }

static inline int rpmh_write_async(struct rpmh_client *rc,
			enum rpmh_state state, struct tcs_cmd *cmd, int n)
{ return -ENODEV; }

static inline int rpmh_write_passthru(struct rpmh_client *rc,
			enum rpmh_state state, struct tcs_cmd *cmd, int *n)
{ return -ENODEV; }

static inline int rpmh_write_control(struct rpmh_client *rc,
			struct tcs_cmd *cmd, int n)
{ return -ENODEV; }

static inline int rpmh_invalidate(struct rpmh_client *rc)
{ return -ENODEV; }

static inline int rpmh_flush(struct rpmh_client *rc)
{ return -ENODEV; }

static inline int rpmh_read(struct rpmh_client *rc, u32 addr,
			u32 *resp)
{ return -ENODEV; }

static inline struct rpmh_client *rpmh_get_byname(struct platform_device *pdev,
			const char *name)
{ return ERR_PTR(-ENODEV); }

static inline struct rpmh_client *rpmh_get_byindex(struct platform_device *pdev,
			int index)
{ return ERR_PTR(-ENODEV); }

static inline void rpmh_release(struct rpmh_client *rc) { }
#endif /* CONFIG_QTI_RPMH_API */

#endif /* __SOC_QCOM_RPMH_H__ */
