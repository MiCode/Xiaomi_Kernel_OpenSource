/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_app_config.h - Configuration of MDDP applications.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifdef CONFIG_MTK_MDDP_USB_SUPPORT
MDDP_MODULE_ID(MDDP_APP_TYPE_USB)
MDDP_MODULE_PREFIX(mddpu)
#endif

#if defined(CONFIG_MTK_MDDP_WH_SUPPORT) || \
	defined(CONFIG_MTK_MCIF_WIFI_SUPPORT)
MDDP_MODULE_ID(MDDP_APP_TYPE_WH)
MDDP_MODULE_PREFIX(mddpwh)
#endif
