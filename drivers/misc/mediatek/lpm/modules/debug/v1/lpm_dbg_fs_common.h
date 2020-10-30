/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_DBG_FS_COMMON_H__
#define __LPM_DBG_FS_COMMON_H__

int lpm_dbg_init(void);
int lpm_dbg_deinit(void);

int lpm_rc_fs_init(void);
int lpm_rc_fs_deinit(void);

int lpm_cpuidle_fs_init(void);
int lpm_cpuidle_fs_deinit(void);

void lpm_cpuidle_state_init(void);
void lpm_cpuidle_profile_init(void);
void lpm_cpuidle_control_init(void);

#endif /* __LPM_DBG_FS_COMMON_H__ */
