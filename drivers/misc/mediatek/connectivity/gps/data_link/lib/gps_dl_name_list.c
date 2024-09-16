/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include "gps_dl_config.h"

#include "gps_dl_base.h"
#include "gps_dl_name_list.h"

#if GPS_DL_ON_LINUX
/* Make sure num for RETURN_NAME_IN_LIST is const to detect coding error such
 * as swapping the position of num and index.
 * MASK_BE_CONST can be empty if compiler not support the macros used.
 */
#define MUST_BE_CONST(num) BUILD_BUG_ON(!__builtin_constant_p(num))
#else
#define MUST_BE_CONST(num)
#endif
#define NAME_ON_NULL "(NULL)"
#define RETURN_NAME_IN_LIST(list, num, index, retval)                          \
	do {                                                                   \
		MUST_BE_CONST(num);                                            \
		if (((index) >= 0) && ((index) < (num))) {                     \
			if ((list)[index])                                     \
				retval = (list)[index];                        \
			else {                                                 \
				GDL_LOGW("name is null for index: %d", index); \
				retval = NAME_ON_NULL;                         \
			}                                                      \
		} else {                                                       \
			GDL_LOGW("name index: %d out of range", index);        \
			retval = (list)[num];                                  \
		}                                                              \
	} while (0)


const char *const gps_dl_ret_name_list[GDL_RET_NUM + 1] = {
	[GDL_OKAY]                = "OKAY",
	[GDL_FAIL]                = "FAIL_GENERAL",
	[GDL_FAIL_ASSERT]         = "FAIL_ASSERT",
	[GDL_FAIL_BUSY]           = "FAIL_BUSY",
	[GDL_FAIL_NOSPACE]        = "FAIL_NOSPACE",
	[GDL_FAIL_NOSPACE_PENDING_RX] = "FAIL_NOSPACE_PENDING_RX",
	[GDL_FAIL_NODATA]         = "FAIL_NODATA",
	[GDL_FAIL_STATE_MISMATCH] = "FAIL_STATE_MISMATCH",
	[GDL_FAIL_SIGNALED]       = "FAIL_SIGNALED",
	[GDL_FAIL_TIMEOUT]        = "FAIL_TIMEOUT",
	[GDL_FAIL_NOT_SUPPORT]    = "FAIL_NOT_SUPPORT",
	[GDL_FAIL_INVAL]          = "FAIL_INVAL",
	[GDL_FAIL_NOENTRY]        = "FAIL_NOENTRY",
	[GDL_FAIL_NOENTRY2]       = "FAIL_NOENTRY2",
	[GDL_FAIL_CONN_NOT_OKAY]  = "FAIL_CONN_NOT_OKAY",
	[GDL_RET_NUM]             = "FAIL_UNKNOWN"
};

const char *gdl_ret_to_name(enum GDL_RET_STATUS gdl_ret)
{
	const char *retval;

	RETURN_NAME_IN_LIST(gps_dl_ret_name_list, GDL_RET_NUM, gdl_ret, retval);
	return retval;
}


const char *const gps_dl_dsp_state_name_list[GPS_DSP_ST_MAX + 1] = {
	[GPS_DSP_ST_OFF]            = "OFF ",
	[GPS_DSP_ST_TURNED_ON]      = "ON  ",
	[GPS_DSP_ST_RESET_DONE]     = "RST ",
	[GPS_DSP_ST_WORKING]        = "WORK",
	[GPS_DSP_ST_HW_SLEEP_MODE]  = "SLP ",
	[GPS_DSP_ST_HW_STOP_MODE]   = "STOP",
	[GPS_DSP_ST_WAKEN_UP]       = "WAKE",
	[GPS_DSP_ST_MAX]            = "UNKN"
};

const char *gps_dl_dsp_state_name(enum gps_dsp_state_t state)
{
	const char *retval;

	RETURN_NAME_IN_LIST(gps_dl_dsp_state_name_list, GPS_DSP_ST_MAX, state, retval);
	return retval;
}


const char *const gps_dl_dsp_event_name_list[GPS_DSP_EVT_MAX + 1] = {
	[GPS_DSP_EVT_FUNC_OFF]          = "FUNC_OFF",
	[GPS_DSP_EVT_FUNC_ON]           = "FUNC_ON ",
	[GPS_DSP_EVT_RESET_DONE]        = "RST_DONE",
	[GPS_DSP_EVT_RAM_CODE_READY]    = "RAM_OKAY",
	[GPS_DSP_EVT_CTRL_TIMER_EXPIRE] = "TIMEOUT ",
	[GPS_DSP_EVT_HW_SLEEP_REQ]      = "SLP_REQ ",
	[GPS_DSP_EVT_HW_SLEEP_EXIT]     = "SLP_WAK ",
	[GPS_DSP_EVT_HW_STOP_REQ]       = "STOP_REQ",
	[GPS_DSP_EVT_HW_STOP_EXIT]      = "STOP_WAK",
	[GPS_DSP_EVT_MAX]               = "UNKNOWN "
};

const char *gps_dl_dsp_event_name(enum gps_dsp_event_t event)
{
	const char *retval;

	RETURN_NAME_IN_LIST(gps_dl_dsp_event_name_list, GPS_DSP_EVT_MAX, event, retval);
	return retval;
}


const char * const gps_dl_link_state_name_list[LINK_STATE_NUM + 1] = {
	[LINK_UNINIT]      = "UNINIT",
	[LINK_CLOSED]      = "CLOSED",
	[LINK_OPENING]     = "OPENING",
	[LINK_OPENED]      = "OPENED",
	[LINK_CLOSING]     = "CLOSING",
	[LINK_RESETTING]   = "RESETTING",
	[LINK_RESET_DONE]  = "RESET_DONE",
	[LINK_DISABLED]    = "DISABLED",
	[LINK_SUSPENDING]  = "SUSPNEDING",
	[LINK_SUSPENDED]   = "SUSPENDED",
	[LINK_RESUMING]    = "RESUMING",
	/* [LNK_INIT_FAIL]   = "INIT_FAIL", */
	[LINK_STATE_NUM]   = "INVALID"
};

const char *gps_dl_link_state_name(enum gps_each_link_state_enum state)
{
	const char *retval;

	RETURN_NAME_IN_LIST(gps_dl_link_state_name_list, LINK_STATE_NUM, state, retval);
	return retval;
}


const char *const gps_dl_link_event_name_list[GPS_DL_LINK_EVT_NUM + 1] = {
	[GPS_DL_EVT_LINK_OPEN]  = "LINK_OPEN",
	[GPS_DL_EVT_LINK_CLOSE] = "LINK_CLOSE",
	[GPS_DL_EVT_LINK_WRITE] = "LINK_WRITE",
	[GPS_DL_EVT_LINK_READ]  = "LINK_READ",
	[GPS_DL_EVT_LINK_DSP_ROM_READY_TIMEOUT] = "ROM_READY_TIMEOUT",
	[GPS_DL_EVT_LINK_DSP_FSM_TIMEOUT] = "DSP_FSM_TIMEOUT",
	[GPS_DL_EVT_LINK_RESET_DSP]       = "RESET_DSP",
	[GPS_DL_EVT_LINK_RESET_GPS]       = "RESET_GPS",
	[GPS_DL_EVT_LINK_PRE_CONN_RESET]  = "PRE_CONN_RESET",
	[GPS_DL_EVT_LINK_POST_CONN_RESET] = "POST_CONN_RESET",
	[GPS_DL_EVT_LINK_PRINT_HW_STATUS] = "PRINT_HW_STATUS",
	[GPS_DL_EVT_LINK_ENTER_DPSLEEP]   = "ENTER_DPSLEEP",
	[GPS_DL_EVT_LINK_LEAVE_DPSLEEP]   = "LEAVE_DPSLEEP",
	[GPS_DL_EVT_LINK_ENTER_DPSTOP]    = "ENTER_DPSTOP",
	[GPS_DL_EVT_LINK_LEAVE_DPSTOP]    = "LEAVE_DPSTOP",
	[GPS_DL_EVT_LINK_PRINT_DATA_STATUS] = "PRINT_DATA_STATUS",
	[GPS_DL_LINK_EVT_NUM]             = "LINK_INVALID_EVT"
};

const char *gps_dl_link_event_name(enum gps_dl_link_event_id event)
{
	const char *retval;

	RETURN_NAME_IN_LIST(gps_dl_link_event_name_list, GPS_DL_LINK_EVT_NUM, event, retval);
	return retval;
}

const char *gps_dl_hal_event_name_list[GPD_DL_HAL_EVT_NUM + 1] = {
	[GPS_DL_HAL_EVT_A2D_TX_DMA_DONE]   = "HAL_TX_DMA_DONE",
	[GPS_DL_HAL_EVT_D2A_RX_HAS_DATA]   = "HAL_RX_HAS_DATA",
	[GPS_DL_HAL_EVT_D2A_RX_HAS_NODATA] = "HAL_RX_HAS_NODATA",
	[GPS_DL_HAL_EVT_D2A_RX_DMA_DONE]   = "HAL_RX_DMA_DONE",
	[GPS_DL_HAL_EVT_MCUB_HAS_IRQ]      = "HAL_MCUB_HAS_FLAG",
	[GPS_DL_HAL_EVT_DMA_ISR_PENDING]   = "HAL_DMA_ISR_PENDING",
	[GPD_DL_HAL_EVT_NUM]               = "HAL_INVALID_EVT",
};

const char *gps_dl_hal_event_name(enum gps_dl_hal_event_id event)
{
	const char *retval;

	RETURN_NAME_IN_LIST(gps_dl_hal_event_name_list, GPD_DL_HAL_EVT_NUM, event, retval);
	return retval;
}

const char *const gps_dl_waitable_name_list[GPS_DL_WAIT_NUM + 1] = {
	[GPS_DL_WAIT_OPEN_CLOSE] = "OPEN_OR_CLOSE",
	[GPS_DL_WAIT_WRITE]      = "WRITE",
	[GPS_DL_WAIT_READ]       = "READ",
	[GPS_DL_WAIT_RESET]      = "RESET",
	[GPS_DL_WAIT_NUM]        = "INVALID"
};

const char *gps_dl_waitable_type_name(enum gps_each_link_waitable_type type)
{
	const char *retval;

	RETURN_NAME_IN_LIST(gps_dl_waitable_name_list, GPS_DL_WAIT_NUM, type, retval);
	return retval;
}

