/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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

struct mc_uuid_t;
struct tbase_object;

int mc_admin_init(struct class *mc_device_class, dev_t *out_dev,
		  int (*tee_start_cb)(void));
void mc_admin_exit(struct class *mc_device_class);

struct tbase_object *tbase_object_select(const struct mc_uuid_t *uuid);
struct tbase_object *tbase_object_get(const struct mc_uuid_t *uuid,
				      uint32_t is_gp_uuid);
struct tbase_object *tbase_object_read(uint32_t spid, uintptr_t address,
				       size_t length);
void tbase_object_free(struct tbase_object *out_robj);

#endif /* ADMIN_FD_H_ */
