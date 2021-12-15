/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TMEM_DEV_DESC_H
#define TMEM_DEV_DESC_H

#include "public/mtee_regions.h"
#include "private/tmem_device.h"
#include "mtee_impl/mtee_ops.h"
#include "tee_impl/tee_ops.h"
#include "tee_impl/tee_regions.h"

struct tmem_device_description {
	enum TRUSTED_MEM_TYPE kern_tmem_type;
	enum TEE_SMEM_TYPE tee_smem_type;
	enum MTEE_MCHUNKS_ID mtee_chunks_id;
	u32 ssmr_feature_id;
	union {
		struct mtee_peer_ops_data mtee;
		struct tee_peer_ops_data tee;
	} u_ops_data;
	bool notify_remote;
	int (*notify_remote_fn)(u64 pa, u32 size, int remote_region_id);
	struct trusted_mem_configs *mem_cfg;
	char *dev_name;
};

#endif /* end of TMEM_DEV_DESC_H */
