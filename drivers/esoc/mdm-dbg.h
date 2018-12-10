/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015, 2018, The Linux Foundation. All rights reserved.
 */

static bool debug_init_done;

#ifndef CONFIG_ESOC_MDM_DBG_ENG

static inline bool dbg_check_cmd_mask(unsigned int cmd)
{
	return false;
}

static inline bool dbg_check_notify_mask(unsigned int notify)
{
	return false;
}

static inline int mdm_dbg_eng_init(struct esoc_drv *drv,
						struct esoc_clink *clink)
{
	return 0;
}

#else
extern bool dbg_check_cmd_mask(unsigned int cmd);
extern bool dbg_check_notify_mask(unsigned int notify);
extern int mdm_dbg_eng_init(struct esoc_drv *drv,
				struct esoc_clink *clink);
#endif

static inline bool mdm_dbg_stall_cmd(unsigned int cmd)
{
	if (debug_init_done)
		return dbg_check_cmd_mask(cmd);
	else
		return false;
}

static inline bool mdm_dbg_stall_notify(unsigned int notify)
{
	if (debug_init_done)
		return dbg_check_notify_mask(notify);
	else
		return false;
}


