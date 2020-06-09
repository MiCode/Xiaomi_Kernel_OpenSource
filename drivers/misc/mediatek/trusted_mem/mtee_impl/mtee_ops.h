/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTEE_OPS_H_
#define MTEE_OPS_H_

struct mtee_driver_params {
	u64 param0;
	u64 param1;
	u64 param2;
	u64 param3;
};

struct mtee_peer_ops_data {
	enum TRUSTED_MEM_TYPE mem_type;
	char *service_name;
};

void get_mtee_peer_ops(struct trusted_driver_operations **ops);

#endif /* MTEE_OPS_H_ */
