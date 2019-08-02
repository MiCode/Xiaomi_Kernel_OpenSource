/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef TEE_TA_MGMT_H
#define TEE_TA_MGMT_H

struct tee;
struct tee_context;

int tee_ta_mgmt_init(void);

int tee_install_sp_ta(struct tee_context *ctx, void __user *ta_spta_inst_desc);
int tee_install_sp_ta_response(struct tee_context *ctx, void __user *u_arg);
int tee_delete_sp_ta(struct tee_context *ctx, void __user *uuid);

int tee_install_sys_ta(struct tee *tee, void __user *u_arg);

#endif
