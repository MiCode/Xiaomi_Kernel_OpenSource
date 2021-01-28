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

#ifndef MTEE_INVOKE_H_
#define MTEE_INVOKE_H_

int mtee_directly_invoke_cmd(struct trusted_driver_cmd_params *invoke_params);
int mtee_set_mchunks_region(u64 pa, u32 size, enum TEE_MEM_TYPE tee_mem_type);

#endif /* MTEE_INVOKE_H_ */
