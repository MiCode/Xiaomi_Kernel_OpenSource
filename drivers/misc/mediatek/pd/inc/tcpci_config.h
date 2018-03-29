/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * Author: TH <tsunghan_tasi@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_TCPC_CONFIG_H
#define __LINUX_TCPC_CONFIG_H

/* default config */

#define CONFIG_PD_DBG_INFO
#define CONFIG_TYPEC_CAP_TRY_SOURCE
#define CONFIG_TYPEC_CAP_TRY_SINK
/* #define CONFIG_TYPEC_CHECK_CC_STABLE */
#define CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_DELAY
#define CONFIG_TYPEC_CHECK_LEGACY_CABLE

#define CONFIG_TYPEC_CAP_RA_DETACH
#define CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG

#define CONFIG_TCPC_VSAFE0V_DETECT
/* #define CONFIG_TCPC_VSAFE0V_DETECT_EXT */
#define CONFIG_TCPC_VSAFE0V_DETECT_IC
#define CONFIG_TCPC_LOW_POWER_MODE
/* #define CONFIG_TCPC_IDLE_MODE */
#define CONFIG_TCPC_CLOCK_GATING

/* #define CONFIG_TCPC_WATCHDOG_EN */
#define CONFIG_TCPC_INTRST_EN

#ifdef CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_SRC_STARTUP_DISCOVER_ID
#define CONFIG_USB_PD_IGNORE_HRESET_COMPLETE_TIMER
#define CONFIG_USB_PD_DROP_REPEAT_PING
#define CONFIG_USB_PD_CHECK_DATA_ROLE
#define CONFIG_USB_PD_RETRY_CRC_DISCARD
#define CONFIG_USB_PD_POSTPONE_VDM
#define CONFIG_USB_PD_POSTPONE_RETRY_VDM
#define CONFIG_USB_PD_POSTPONE_FIRST_VDM
#define CONFIG_USB_PD_POSTPONE_OTHER_VDM
#define CONFIG_USB_PD_SAFE0V_DELAY

#ifndef CONFIG_USB_PD_VBUS_STABLE_TOUT
#define CONFIG_USB_PD_VBUS_STABLE_TOUT 125
#endif
#endif /* CONFIG_USB_POWER_DELIVERY */

/* debug config */
#define CONFIG_USB_PD_DBG_ALERT_STATUS
/* #define CONFIG_USB_PD_DBG_SKIP_ALERT_HANDLER */

#endif /* __LINUX_TCPC_CONFIG_H */
