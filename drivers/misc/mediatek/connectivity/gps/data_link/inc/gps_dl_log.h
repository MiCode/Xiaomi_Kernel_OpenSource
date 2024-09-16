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
#ifndef _GPS_DL_LOG_H
#define _GPS_DL_LOG_H

#include "gps_dl_config.h"
#if GPS_DL_ON_LINUX
#include <linux/printk.h>
#elif GPS_DL_ON_CTP
#include "gps_dl_ctp_log.h"
#endif

enum gps_dl_log_level_enum {
	GPS_DL_LOG_LEVEL_DBG  = 1,
	GPS_DL_LOG_LEVEL_INFO = 3,
	GPS_DL_LOG_LEVEL_WARN = 5,
	GPS_DL_LOG_LEVEL_ERR  = 7,
};

enum gps_dl_log_module_enum {
	GPS_DL_LOG_MOD_DEFAULT    = 0,
	GPS_DL_LOG_MOD_OPEN_CLOSE = 1,
	GPS_DL_LOG_MOD_READ_WRITE = 2,
	GPS_DL_LOG_MOD_REG_RW     = 3,
	GPS_DL_LOG_MOD_STATUS     = 4,
	GPS_DL_LOG_MOD_EVENT      = 5,
	GPS_DL_LOG_MOD_INIT       = 6,

	GPS_DL_LOG_MOD_MAX        = 32
};

enum gps_dl_log_reg_rw_ctrl_enum {
	GPS_DL_REG_RW_HOST_CSR_PTA_INIT,
	GPS_DL_REG_RW_HOST_CSR_GPS_OFF,
	GPS_DL_REG_RW_EMI_SW_REQ_CTRL,
	GPS_DL_REG_RW_MCUB_IRQ_HANDLER,

	GPS_DL_REG_RW_MAX = 32
};

#define GPS_DL_LOG_DEF_SETTING_LEVEL   GPS_DL_LOG_LEVEL_INFO
#define GPS_DL_LOG_DEF_SETTING_MODULES (             \
		(1UL << GPS_DL_LOG_MOD_DEFAULT)    | \
		(1UL << GPS_DL_LOG_MOD_OPEN_CLOSE) | \
		(0UL << GPS_DL_LOG_MOD_READ_WRITE) | \
		(1UL << GPS_DL_LOG_MOD_REG_RW)     | \
		(1UL << GPS_DL_LOG_MOD_STATUS)     | \
		(0UL << GPS_DL_LOG_MOD_EVENT)      | \
		(1UL << GPS_DL_LOG_MOD_INIT)       | \
	0)
#define GPS_DL_LOG_REG_RW_BITMASK (                       \
		(0UL << GPS_DL_REG_RW_HOST_CSR_PTA_INIT) |\
		(0UL << GPS_DL_REG_RW_HOST_CSR_GPS_OFF)  |\
		(0UL << GPS_DL_REG_RW_EMI_SW_REQ_CTRL)   |\
		(0UL << GPS_DL_REG_RW_MCUB_IRQ_HANDLER)  |\
	0)

enum gps_dl_log_level_enum gps_dl_log_level_get(void);
void gps_dl_log_level_set(enum gps_dl_log_level_enum level);

unsigned int gps_dl_log_mod_bitmask_get(void);
void gps_dl_log_mod_bitmask_set(unsigned int bitmask);
bool gps_dl_log_mod_is_on(enum gps_dl_log_module_enum mod);
void gps_dl_log_mod_on(enum gps_dl_log_module_enum mod);
void gps_dl_log_mod_off(enum gps_dl_log_module_enum mod);

bool gps_dl_log_reg_rw_is_on(enum gps_dl_log_reg_rw_ctrl_enum log_reg_rw);

void gps_dl_log_info_show(void);


#if GPS_DL_ON_LINUX
#define __GDL_LOGE(mod, fmt, ...) pr_notice("GDL[E:%d] [%s:%d]: "fmt, \
	mod, __func__, __LINE__, ##__VA_ARGS__)
#define __GDL_LOGW(mod, fmt, ...) pr_notice("GDL[W:%d] [%s:%d]: "fmt, \
	mod, __func__, __LINE__, ##__VA_ARGS__)
#define __GDL_LOGI(mod, fmt, ...) pr_info("GDL[I:%d] [%s:%d]: "fmt, \
	mod, __func__, __LINE__, ##__VA_ARGS__)
#define __GDL_LOGD(mod, fmt, ...) pr_info("GDL[D:%d] [%s:%d]: "fmt, \
	mod, __func__, __LINE__, ##__VA_ARGS__)

#define __GDL_LOGXE(mod, link_id, fmt, ...) pr_notice("GDL-%d[E:%d] [%s:%d]: "fmt, \
	link_id, mod, __func__, __LINE__, ##__VA_ARGS__)

#define __GDL_LOGXW(mod, link_id, fmt, ...) pr_notice("GDL-%d[W:%d] [%s:%d]: "fmt, \
	link_id, mod, __func__, __LINE__, ##__VA_ARGS__)

#define __GDL_LOGXI(mod, link_id, fmt, ...) pr_info("GDL-%d[I:%d] [%s:%d]: "fmt, \
	link_id, mod, __func__, __LINE__, ##__VA_ARGS__)

#define __GDL_LOGXD(mod, link_id, fmt, ...) pr_info("GDL-%d[D:%d] [%s:%d]: "fmt, \
	link_id, mod, __func__, __LINE__, ##__VA_ARGS__)
#endif /* GPS_DL_ON_XX */


#define _GDL_LOGE(...) \
	do { if (gps_dl_log_level_get() <= GPS_DL_LOG_LEVEL_ERR) __GDL_LOGE(__VA_ARGS__); } while (0)
#define _GDL_LOGW(...) \
	do { if (gps_dl_log_level_get() <= GPS_DL_LOG_LEVEL_WARN) __GDL_LOGW(__VA_ARGS__); } while (0)
#define _GDL_LOGI(...) \
	do { if (gps_dl_log_level_get() <= GPS_DL_LOG_LEVEL_INFO) __GDL_LOGI(__VA_ARGS__); } while (0)
#define _GDL_LOGD(...) \
	do { if (gps_dl_log_level_get() <= GPS_DL_LOG_LEVEL_DBG) __GDL_LOGD(__VA_ARGS__); } while (0)
#define _GDL_LOGXE(...) \
	do { if (gps_dl_log_level_get() <= GPS_DL_LOG_LEVEL_ERR) __GDL_LOGXE(__VA_ARGS__); } while (0)
#define _GDL_LOGXW(...) \
	do { if (gps_dl_log_level_get() <= GPS_DL_LOG_LEVEL_WARN) __GDL_LOGXW(__VA_ARGS__); } while (0)
#define _GDL_LOGXI(...) \
	do { if (gps_dl_log_level_get() <= GPS_DL_LOG_LEVEL_INFO) __GDL_LOGXI(__VA_ARGS__); } while (0)
#define _GDL_LOGXD(...) \
	do { if (gps_dl_log_level_get() <= GPS_DL_LOG_LEVEL_DBG) __GDL_LOGXD(__VA_ARGS__); } while (0)


#define GDL_LOGE2(mod, ...) \
	do { if (1) \
		_GDL_LOGE(mod, __VA_ARGS__); } while (0)
#define GDL_LOGW2(mod, ...) \
	do { if (1) \
		_GDL_LOGW(mod, __VA_ARGS__); } while (0)
#define GDL_LOGI2(mod, ...) \
	do { if (gps_dl_log_mod_is_on(mod)) \
		_GDL_LOGI(mod, __VA_ARGS__); } while (0)
#define GDL_LOGD2(mod, ...) \
	do { if (gps_dl_log_mod_is_on(mod)) \
		_GDL_LOGD(mod, __VA_ARGS__); } while (0)

/* Usage:
 * 1. Bellow macro can be used to output log:
 *       err  level: GDL_LOGE, GDL_LOGE_YYY, GDL_LOGXE, GDL_LOGXE_YYY
 *       warn level: GDL_LOGW, GDL_LOGW_YYY, GDL_LOGXW, GDL_LOGXW_YYY
 *       info level: GDL_LOGI, GDL_LOGI_YYY, GDL_LOGXI, GDL_LOGXI_YYY
 *       dbg  level: GDL_LOGD, GDL_LOGE_YYY, GDL_LOGXD, GDL_LOGXD_YYY
 *
 * 2. _YYY stands for log module(group), the list are:
 *       _ONF for device/link open or close flow
 *       _DRW for devcie/link read or write flow
 *       _RRW for hw register read or write flow
 *       _STA for state machine related flow
 *       _EVT for event processing related flow
 *       _INI for device initialization/deinitializaion flow
 *    if they are used, the log can be easily filtered by keywords like "[E:2]", "[I:5]" and so on
 *    if you don't know which to use, just use: GDL_LOG* or GDL_LOGX*, which have no _YYY subfix
 *
 * 3. Log of info and dbg level can be showed seeing log level and module setting meet:
 *       a) log level setting <= INFO or DBG and
 *       b) log module bitmask bit is 1 for the module
 *
 * 4. Log of warn and err level is showed only seeing log level:
 *       a) log level setting <= WARN or ERR
 *
 * 5. Difference between GDL_LOG* and GDL_LOGX*:
 *      GDL_LOG* can be used like: GDL_LOGD("a = %d", a)
 *      GDL_LOGX* can take a parameters of link_id, like: GDL_LOGXD(link_id, "a = %d", a)
 */
#define GDL_LOGE(...) GDL_LOGE2(GPS_DL_LOG_MOD_DEFAULT, __VA_ARGS__)
#define GDL_LOGW(...) GDL_LOGW2(GPS_DL_LOG_MOD_DEFAULT, __VA_ARGS__)
#define GDL_LOGI(...) GDL_LOGI2(GPS_DL_LOG_MOD_DEFAULT, __VA_ARGS__)
#define GDL_LOGD(...) GDL_LOGD2(GPS_DL_LOG_MOD_DEFAULT, __VA_ARGS__)

#define GDL_LOGD_ONF(...) GDL_LOGD2(GPS_DL_LOG_MOD_OPEN_CLOSE, __VA_ARGS__)

#define GDL_LOGI_DRW(...) GDL_LOGI2(GPS_DL_LOG_MOD_READ_WRITE, __VA_ARGS__)

#define GDL_LOGW_RRW(...) GDL_LOGI2(GPS_DL_LOG_MOD_REG_RW, __VA_ARGS__)
#define GDL_LOGI_RRW(...) GDL_LOGI2(GPS_DL_LOG_MOD_REG_RW, __VA_ARGS__)

#define GDL_LOGE_EVT(...) GDL_LOGE2(GPS_DL_LOG_MOD_EVENT, __VA_ARGS__)
#define GDL_LOGW_EVT(...) GDL_LOGW2(GPS_DL_LOG_MOD_EVENT, __VA_ARGS__)
#define GDL_LOGD_EVT(...) GDL_LOGD2(GPS_DL_LOG_MOD_EVENT, __VA_ARGS__)

#define GDL_LOGE_INI(...) GDL_LOGE2(GPS_DL_LOG_MOD_INIT, __VA_ARGS__)
#define GDL_LOGW_INI(...) GDL_LOGW2(GPS_DL_LOG_MOD_INIT, __VA_ARGS__)
#define GDL_LOGI_INI(...) GDL_LOGI2(GPS_DL_LOG_MOD_INIT, __VA_ARGS__)
#define GDL_LOGD_INI(...) GDL_LOGD2(GPS_DL_LOG_MOD_INIT, __VA_ARGS__)

#define GDL_LOGXE2(mod, ...) \
	do { if (1) \
		_GDL_LOGXE(mod, __VA_ARGS__); } while (0)
#define GDL_LOGXW2(mod, ...) \
	do { if (1) \
		_GDL_LOGXW(mod, __VA_ARGS__); } while (0)
#define GDL_LOGXI2(mod, ...) \
	do { if (gps_dl_log_mod_is_on(mod)) \
		_GDL_LOGXI(mod, __VA_ARGS__); } while (0)
#define GDL_LOGXD2(mod, ...) \
	do { if (gps_dl_log_mod_is_on(mod)) \
		_GDL_LOGXD(mod, __VA_ARGS__); } while (0)

#define GDL_LOGXE(...) GDL_LOGXE2(GPS_DL_LOG_MOD_DEFAULT, __VA_ARGS__)
#define GDL_LOGXW(...) GDL_LOGXW2(GPS_DL_LOG_MOD_DEFAULT, __VA_ARGS__)
#define GDL_LOGXI(...) GDL_LOGXI2(GPS_DL_LOG_MOD_DEFAULT, __VA_ARGS__)
#define GDL_LOGXD(...) GDL_LOGXD2(GPS_DL_LOG_MOD_DEFAULT, __VA_ARGS__)

#define GDL_LOGXE_ONF(...) GDL_LOGXE2(GPS_DL_LOG_MOD_OPEN_CLOSE, __VA_ARGS__)
#define GDL_LOGXW_ONF(...) GDL_LOGXW2(GPS_DL_LOG_MOD_OPEN_CLOSE, __VA_ARGS__)
#define GDL_LOGXI_ONF(...) GDL_LOGXI2(GPS_DL_LOG_MOD_OPEN_CLOSE, __VA_ARGS__)
#define GDL_LOGXD_ONF(...) GDL_LOGXD2(GPS_DL_LOG_MOD_OPEN_CLOSE, __VA_ARGS__)

#define GDL_LOGXE_DRW(...) GDL_LOGXE2(GPS_DL_LOG_MOD_READ_WRITE, __VA_ARGS__)
#define GDL_LOGXW_DRW(...) GDL_LOGXW2(GPS_DL_LOG_MOD_READ_WRITE, __VA_ARGS__)
#define GDL_LOGXI_DRW(...) GDL_LOGXI2(GPS_DL_LOG_MOD_READ_WRITE, __VA_ARGS__)
#define GDL_LOGXD_DRW(...) GDL_LOGXD2(GPS_DL_LOG_MOD_READ_WRITE, __VA_ARGS__)

#define GDL_LOGXE_STA(...) GDL_LOGXE2(GPS_DL_LOG_MOD_STATUS, __VA_ARGS__)
#define GDL_LOGXW_STA(...) GDL_LOGXW2(GPS_DL_LOG_MOD_STATUS, __VA_ARGS__)
#define GDL_LOGXI_STA(...) GDL_LOGXI2(GPS_DL_LOG_MOD_STATUS, __VA_ARGS__)

#define GDL_LOGXW_EVT(...) GDL_LOGXW2(GPS_DL_LOG_MOD_EVENT, __VA_ARGS__)
#define GDL_LOGXI_EVT(...) GDL_LOGXI2(GPS_DL_LOG_MOD_EVENT, __VA_ARGS__)
#define GDL_LOGXD_EVT(...) GDL_LOGXD2(GPS_DL_LOG_MOD_EVENT, __VA_ARGS__)

#define GDL_LOGXE_INI(...) GDL_LOGXE2(GPS_DL_LOG_MOD_INIT, __VA_ARGS__)
#define GDL_LOGXI_INI(...) GDL_LOGXI2(GPS_DL_LOG_MOD_INIT, __VA_ARGS__)
#define GDL_LOGXD_INI(...) GDL_LOGXD2(GPS_DL_LOG_MOD_INIT, __VA_ARGS__)


#define GDL_ASSERT(cond, ret, fmt, ...)                   \
	do { if (!(cond)) {                                   \
		GDL_LOGE("{** GDL_ASSERT: [(%s) = %d] **}: "fmt,  \
			#cond, (cond), ##__VA_ARGS__);                \
		return ret;                                       \
	} } while (0)

void GDL_VOIDF(void);

#define GDL_ASSERT_RET_OKAY(gdl_ret) \
	GDL_ASSERT((callee_ret) != GDL_OKAY, GDL_FAIL_ASSERT, "")

#define GDL_ASSERT_RET_NOT_ASSERT(callee_ret, ret_to_caller) \
	GDL_ASSERT((callee_ret) == GDL_FAIL_ASSERT, ret_to_caller, "")

#define ASSERT_NOT_ZERO(val, ret)\
	GDL_ASSERT(val != 0, ret, "%s should not be 0!", #val)

#define ASSERT_ZERO(val, ret)\
	GDL_ASSERT(val == 0, ret, "%s should be 0!", #val)

#define ASSERT_NOT_NULL(ptr, ret)\
	GDL_ASSERT(ptr != NULL, ret, "null ptr!")

#define ASSERT_LINK_ID(link_id, ret) \
	GDL_ASSERT(LINK_ID_IS_VALID(link_id), ret, "invalid link_id: %d", link_id)

#endif /* _GPS_DL_LOG_H */

