/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef __TEE_SYSFS_H__
#define __TEE_SYSFS_H__

struct tee;

int tee_init_sysfs(struct tee *tee);
void tee_cleanup_sysfs(struct tee *tee);

#endif
