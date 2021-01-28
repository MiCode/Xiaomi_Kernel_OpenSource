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

#ifndef MTEE_PRIVATE_H_
#define MTEE_PRIVATE_H_

struct mtee_peer_ops_priv_data {
	enum TRUSTED_MEM_TYPE mem_type;
};

void get_mtee_peer_ops(struct trusted_driver_operations **ops);

#endif /* MTEE_PRIVATE_H_ */
