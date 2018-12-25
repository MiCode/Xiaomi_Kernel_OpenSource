/*
 * Copyright (c) 2013-2016 TRUSTONIC LIMITED
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

#ifndef _MC_ADMIN_H_
#define _MC_ADMIN_H_

struct cdev;
struct mc_uuid_t;
struct tee_object;

int mc_admin_init(struct cdev *cdev, int (*tee_start_cb)(void),
		  void (*tee_stop_cb)(void));
void mc_admin_exit(void);

struct tee_object *tee_object_select(const struct mc_uuid_t *uuid);
struct tee_object *tee_object_get(const struct mc_uuid_t *uuid, bool is_gp);
struct tee_object *tee_object_read(u32 spid, uintptr_t address, size_t length);
void tee_object_free(struct tee_object *object);
int is_authenticator_pid(pid_t pid);

#endif /* _MC_ADMIN_H_ */
