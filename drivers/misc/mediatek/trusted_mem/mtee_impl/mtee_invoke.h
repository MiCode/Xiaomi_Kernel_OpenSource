/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTEE_INVOKE_H_
#define MTEE_INVOKE_H_

int mtee_directly_invoke_cmd(struct trusted_driver_cmd_params *invoke_params);
int mtee_set_mchunks_region(u64 pa, u32 size, int remote_region_type);

#endif /* MTEE_INVOKE_H_ */
