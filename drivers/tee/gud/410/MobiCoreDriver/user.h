/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#ifndef _USER_H_
#define _USER_H_

struct cdev;

int mc_user_init(struct cdev *cdev);
static inline void mc_user_exit(void)
{
}

#endif /* _USER_H_ */
