/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef HACC_TEE_H
#define HACC_TEE_H

extern u32 get_devinfo_with_index(u32 index);
int open_sdriver_connection(void);
int tee_secure_request(unsigned int user, unsigned char *data, unsigned int data_size,
		       unsigned int direction, unsigned char *seed, unsigned int seed_size);
int close_sdriver_connection(void);

#endif
