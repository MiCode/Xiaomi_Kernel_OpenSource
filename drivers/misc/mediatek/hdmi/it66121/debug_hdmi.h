/*
 * HDMI support
 *
 * Copyright (C) 2013 ITE Tech. Inc.
 * Author: Hermes Wu <hermes.wu@ite.com.tw>
 *
 * HDMI TX driver for IT66121
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_
#define Debug_message 1
/* #pragma message("debug.h") */
#define HDMITX_DEBUG_PRINTF(fmt, arg...) printk(fmt, ##arg)
#define HDMITX_DEBUG_PRINTF1(fmt, arg...) printk(fmt, ##arg)
#define HDMITX_DEBUG_PRINTF2(fmt, arg...) printk(fmt, ##arg)
#define HDMITX_DEBUG_PRINTF3(fmt, arg...) printk(fmt, ##arg)

#define HDCP_DEBUG_PRINTF(fmt, arg...)  /* printk(fmt, ##arg) */
#define HDCP_DEBUG_PRINTF1(fmt, arg...) /* printk(fmt, ##arg) */
#define HDCP_DEBUG_PRINTF2(fmt, arg...) /* printk(fmt, ##arg) */
#define HDCP_DEBUG_PRINTF3(fmt, arg...) /* printk(fmt, ##arg) */

#if 0
#define EDID_DEBUG_PRINTF(fmt, arg...) printk(fmt, ##arg)
#define EDID_DEBUG_PRINTF1(fmt, arg...) printk(fmt, ##arg)
#define EDID_DEBUG_PRINTF2(fmt, arg...) printk(fmt, ##arg)
#define EDID_DEBUG_PRINTF3(fmt, arg...) printk(fmt, ##arg)

#else
#define EDID_DEBUG_LOG_ON 0
#define EDID_DEBUG_PRINTF(fmt, arg...)                                         \
	do {                                                                   \
		if (EDID_DEBUG_LOG_ON) {                                       \
			pr_debug("[EDID]%s,%d ", __func__, __LINE__);          \
			pr_debug(fmt, ##arg);                                  \
		}                                                              \
	} while (0)

#define EDID_DEBUG_PRINTF1(fmt, arg...)                                        \
	do {                                                                   \
		if (EDID_DEBUG_LOG_ON) {                                       \
			pr_debug("[EDID]%s,%d ", __func__, __LINE__);          \
			pr_debug(fmt, ##arg);                                  \
		}                                                              \
	} while (0)

#define EDID_DEBUG_PRINTF2(fmt, arg...)                                        \
	do {                                                                   \
		if (EDID_DEBUG_LOG_ON) {                                       \
			pr_debug("[EDID]%s,%d ", __func__, __LINE__);          \
			pr_debug(fmt, ##arg);                                  \
		}                                                              \
	} while (0)

#define EDID_DEBUG_PRINTF3(fmt, arg...)                                        \
	do {                                                                   \
		if (EDID_DEBUG_LOG_ON) {                                       \
			pr_debug("[EDID]%s,%d ", __func__, __LINE__);          \
			pr_debug(fmt, ##arg);                                  \
		}                                                              \
	} while (0)

#endif

#define HDMITX_DEBUG_INFO

#define IT66121_LOG_on 0
#define IT66121_LOG(fmt, arg...)                                               \
	do {                                                                   \
		if (IT66121_LOG_on) {                                          \
			pr_debug("[hdmi_it66121]%s,%d ", __func__, __LINE__);  \
			pr_debug(fmt, ##arg);                                  \
		}                                                              \
	} while (0)

#define it66121_FUNC()                                                         \
	do {                                                                   \
		if (IT66121_LOG_on) {                                          \
			pr_debug("[hdmi_it66121] %s\n", __func__);             \
		}                                                              \
	} while (0)

#endif /* _DEBUG_H_ */
