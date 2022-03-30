/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __ADAPTOR_CTRLS_H__
#define __ADAPTOR_CTRLS_H__

int adaptor_init_ctrls(struct adaptor_ctx *ctx);

void restore_ae_ctrl(struct adaptor_ctx *ctx);

void adaptor_sensor_init(struct adaptor_ctx *ctx);

/* callback function for frame-sync set framelength using */
/*     return: 0 => No-Error ; non-0 => Error */
int cb_fsync_mgr_set_framelength(void *p_ctx,
				unsigned int cmd_id,
				unsigned int framelength);

void fsync_setup_hdr_exp_data(struct adaptor_ctx *ctx,
				struct fs_hdr_exp_st *p_hdr_exp,
				u32 ae_exp_cnt,
				u32 *ae_exp_arr,
				u32 fine_integ_line);

#endif
