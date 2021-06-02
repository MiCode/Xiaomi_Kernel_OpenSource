/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_debug.h - Public API/structure provided for logging.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __MDDP_DEBUG_H
#define __MDDP_DEBUG_H

//------------------------------------------------------------------------------
// Define marco - log level/class.
// -----------------------------------------------------------------------------
extern uint32_t mddp_debug_log_class_s;
extern uint32_t mddp_debug_log_level_s;
#define _MDDP_DEBUG(_class, _lv, _fmt, _args...) \
	do { \
		if ((mddp_debug_log_class_s & _class) && \
			(mddp_debug_log_level_s >= _lv)) \
			pr_notice(_fmt, ##_args); \
	} while (0)

#define MDDP_C_LOG(_lv, _fmt, _args...) \
	_MDDP_DEBUG(MDDP_LC_CTRL, _lv, _fmt, _args)
#define MDDP_S_LOG(_lv, _fmt, _args...) \
	_MDDP_DEBUG(MDDP_LC_SM, _lv, _fmt, _args)
#define MDDP_F_LOG(_lv, _fmt, _args...) \
	_MDDP_DEBUG(MDDP_LC_FILTER, _lv, _fmt, _args)
#define MDDP_U_LOG(_lv, _fmt, _args...) \
	_MDDP_DEBUG(MDDP_LC_USAGE, _lv, _fmt, _args)

//------------------------------------------------------------------------------
// Struct definition - log level/class.
// -----------------------------------------------------------------------------
#define MDDP_DEBUG_LOG_CLASS_MASK       0xF0
#define MDDP_IS_VALID_LOG_CLASS(_class) \
	((_class >= MDDP_LC_OFF) && (_class <= MDDP_LC_ALL))
enum mddp_log_class_e {
	MDDP_LC_OFF = 0,                    /* 0 */
	MDDP_LC_CTRL = 0x1,                 /* 1 */
	MDDP_LC_SM = 0x2,                   /* 2 */
	MDDP_LC_FILTER = 0x4,               /* 4 */
	MDDP_LC_USAGE = 0x8,                /* 8 */
	MDDP_LC_ALL = 0xF,                  /* F */
};

#define MDDP_DEBUG_LOG_LV_MASK          0x0F
#define MDDP_IS_VALID_LOG_LEVEL(_level) \
	((_level >= MDDP_LL_CRIT) && (_level <= MDDP_LL_ALL))
enum mddp_log_level_e {
	MDDP_LL_CRIT,                       /* 0 */
	MDDP_LL_ERR,                        /* 1 */
	MDDP_LL_WARN,                       /* 2 */
	MDDP_LL_NOTICE,                     /* 3 */
	MDDP_LL_INFO,                       /* 4 */
	MDDP_LL_DEBUG,                      /* 5 */
	MDDP_LL_ALL,                        /* 6 */
};
#define MDDP_LL_ENG_DEF                 MDDP_LL_NOTICE
#define MDDP_LL_NON_ENG_DEF             MDDP_LL_WARN

//------------------------------------------------------------------------------
// Define marco.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
enum mddp_dstate_id_e {
	MDDP_DSTATE_ID_START,
	MDDP_DSTATE_ID_STOP,
	MDDP_DSTATE_ID_NEW_TAG,
	MDDP_DSTATE_ID_SUSPEND_TAG,
	MDDP_DSTATE_ID_RESUME_TAG,
	MDDP_DSTATE_ID_GET_OFFLOAD_STATS,

	MDDP_DSTATE_ID_NUM,
};

#define MDDP_DSTATE_STR_SZ 96
struct mddp_dstate_t {
	enum mddp_dstate_id_e   id;
	uint8_t                 str[MDDP_DSTATE_STR_SZ];
};

static struct mddp_dstate_t mddp_dstate_temp_s[] = {
	{MDDP_DSTATE_ID_START,
	"==================== [%s] START DSTATE ===================="},
	{MDDP_DSTATE_ID_STOP,
	"==================== [%s] STOP DSTATE ===================="},
	{MDDP_DSTATE_ID_NEW_TAG,
	"[%s] New connection, ip(%x) port(%d)"},
	{MDDP_DSTATE_ID_SUSPEND_TAG,
	"[%s] Suspend tag"},
	{MDDP_DSTATE_ID_RESUME_TAG,
	"[%s] Resume tag"},
	{MDDP_DSTATE_ID_GET_OFFLOAD_STATS,
	"[%s] Get offload stats, rx(%08llu), tx(%08llu)"},
};

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------

#endif /* __MDDP_DEBUG_H */
