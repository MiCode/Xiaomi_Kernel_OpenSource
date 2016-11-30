/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

static inline int mdm_dbg_eng_init(struct esoc_drv *drv)
{
	return 0;
}

#else
extern bool dbg_check_cmd_mask(unsigned int cmd);
extern bool dbg_check_notify_mask(unsigned int notify);
extern int mdm_dbg_eng_init(struct esoc_drv *drv);
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


