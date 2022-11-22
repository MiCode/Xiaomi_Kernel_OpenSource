/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __ADAPTOR_CTRLS_H__
#define __ADAPTOR_CTRLS_H__

int adaptor_init_ctrls(struct adaptor_ctx *ctx);

void restore_ae_ctrl(struct adaptor_ctx *ctx);

void adaptor_sensor_init(struct adaptor_ctx *ctx);

u32 get_mode_vb(struct adaptor_ctx *ctx, const struct sensor_mode *mode);

#endif
