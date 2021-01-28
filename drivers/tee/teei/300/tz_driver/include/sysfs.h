/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef __TEEI_SYS_FS_H__
#define __TEEI_SYS_FS_H__
int init_sysfs(struct platform_device *pdev);
void remove_sysfs(struct platform_device *pdev);

#endif
