/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_CONSOLE_H
#define _FRAME_SYNC_CONSOLE_H


#include "frame_sync_def.h"


/******************************************************************************/
// function used internally by frame-sync
/******************************************************************************/
unsigned int fs_con_chk_force_to_ignore_set_sync(void);

unsigned int fs_con_chk_default_en_set_sync(void);

/* if enable => *p_val will be overwrite to user config value */
unsigned int fs_con_chk_en_overwrite_set_sync(const unsigned int sidx,
	unsigned int *p_val);

/* if enable => *p_val will be overwrite to user config value */
unsigned int fs_con_chk_en_overwrite_max_fps(const unsigned int sidx,
	unsigned int *p_val);

int fs_con_chk_usr_async_m_sidx(void);

unsigned int fs_con_get_usr_listen_ext_vsync(void);
unsigned int fs_con_get_usr_auto_listen_ext_vsync(void);
unsigned int fs_con_get_listen_vsync_alg_cfg(void);
void fs_con_set_listen_vsync_alg_cfg(unsigned int flag);





/******************************************************************************/
// sysfs create/remove file
/******************************************************************************/
int fs_con_create_sysfs_file(struct device *dev);
void fs_con_remove_sysfs_file(struct device *dev);

#endif
