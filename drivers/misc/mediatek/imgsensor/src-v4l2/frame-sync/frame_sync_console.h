/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_CONSOLE_H
#define _FRAME_SYNC_CONSOLE_H


#include "frame_sync_def.h"


unsigned int fs_con_check_usr_disable_sync(void);
unsigned int fs_con_get_usr_listen_ext_vsync(void);
unsigned int fs_con_get_usr_auto_listen_ext_vsync(void);
unsigned int fs_con_get_listen_vsync_alg_cfg(void);
void fs_con_set_listen_vsync_alg_cfg(unsigned int flag);


int fs_con_create_sysfs_file(struct device *dev);
void fs_con_remove_sysfs_file(struct device *dev);

#endif
