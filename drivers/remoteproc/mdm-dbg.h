/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015, 2018, 2020-2021, The Linux Foundation. All rights reserved.
 */

static bool debug_init_done;

#ifndef CONFIG_QCOM_ESOC_DBG_ENG

static inline bool dbg_check_cmd_mask(unsigned int cmd)
{
	return false;
}

static inline bool dbg_check_notify_mask(unsigned int notify)
{
	return false;
}

static inline int mdm_dbg_eng_init(void)
{
	return 0;
}

static inline void mdm_dbg_eng_exit(void)
{
}

#else
extern bool dbg_check_cmd_mask(unsigned int cmd);
extern bool dbg_check_notify_mask(unsigned int notify);
extern int mdm_dbg_eng_init(void);
extern void mdm_dbg_eng_exit(void);
#endif

#ifndef CONFIG_QCOM_ESOC_DBG_REQ_ENG
static inline int register_dbg_req_eng(struct esoc_clink *clink)
{
	return 0;
}
static inline void unregister_dbg_req_eng(struct esoc_clink *clink)
{
}
#else
extern int register_dbg_req_eng(struct esoc_clink *clink);
extern void unregister_dbg_req_eng(struct esoc_clink *clink);
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


