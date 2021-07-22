/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifdef CONFIG_RPMB_INTF_DEV
int __init rpmb_cdev_init(void);
void __exit rpmb_cdev_exit(void);
void rpmb_cdev_prepare(struct rpmb_dev *rdev);
void rpmb_cdev_add(struct rpmb_dev *rdev);
void rpmb_cdev_del(struct rpmb_dev *rdev);
#else
static inline int __init rpmb_cdev_init(void) { return 0; }
static inline void __exit rpmb_cdev_exit(void) {}
static inline void rpmb_cdev_prepare(struct rpmb_dev *rdev) {}
static inline void rpmb_cdev_add(struct rpmb_dev *rdev) {}
static inline void rpmb_cdev_del(struct rpmb_dev *rdev) {}
#endif /* CONFIG_RPMB_INTF_DEV */
