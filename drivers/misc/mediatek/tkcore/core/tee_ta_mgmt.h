/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
