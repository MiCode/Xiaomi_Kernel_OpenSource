/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef __SHAREDMEM_QMI_H__
#define __SHAREDMEM_QMI_H__

#include <linux/module.h>

struct sharemem_qmi_entry {
	const char *client_name;
	u32 client_id;
	u64 address;
	u32 size;
	bool is_addr_dynamic;
};

int sharedmem_qmi_init(void);

void sharedmem_qmi_exit(void);

void sharedmem_qmi_add_entry(struct sharemem_qmi_entry *qmi_entry);

#endif /* __SHAREDMEM_QMI_H__ */
