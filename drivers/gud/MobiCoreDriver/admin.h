/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
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

#ifndef ADMIN_FD_H_
#define ADMIN_FD_H_

#include <public/mc_linux.h>

struct tbase_object {
	uint32_t	length;		/* Total length */
	uint32_t	header_length;	/* Length of header before payload */
	uint8_t		data[];		/* Header followed by payload */
};

int admin_dev_init(struct class *mc_device_class, dev_t *out_dev);
void admin_dev_cleanup(struct class *mc_device_class);

struct tbase_object *tbase_object_select(const struct mc_uuid_t *uuid);
struct tbase_object *tbase_object_get(const struct mc_uuid_t *uuid,
				      uint32_t is_gp_uuid);
struct tbase_object *tbase_object_read(uint32_t spid, uintptr_t address,
				       size_t length);
void tbase_object_free(struct tbase_object *out_robj);

#endif /* ADMIN_FD_H_ */
