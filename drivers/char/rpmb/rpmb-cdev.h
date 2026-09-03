/*
 * Copyright (C) 2015-2016 Intel Corp. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */
#if IS_ENABLED(CONFIG_RPMB_INTF_DEV)
int rpmb_cdev_init(void);
void rpmb_cdev_exit(void);
void rpmb_cdev_prepare(struct rpmb_dev *rdev);
void rpmb_cdev_add(struct rpmb_dev *rdev);
void rpmb_cdev_del(struct rpmb_dev *rdev);
#else
static inline int rpmb_cdev_init(void) { return 0; }
static inline void rpmb_cdev_exit(void) {}
static inline void rpmb_cdev_prepare(struct rpmb_dev *rdev) {}
static inline void rpmb_cdev_add(struct rpmb_dev *rdev) {}
static inline void rpmb_cdev_del(struct rpmb_dev *rdev) {}
#endif /* CONFIG_RPMB_INTF_DEV */
