/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
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
struct tee_object *tee_object_copy(uintptr_t address, size_t length);
struct tee_object *tee_object_read(u32 spid, uintptr_t address, size_t length);
void tee_object_free(struct tee_object *object);

#endif /* _MC_ADMIN_H_ */
