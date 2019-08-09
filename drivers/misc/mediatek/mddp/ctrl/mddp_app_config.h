/*
 * mddp_app_config.h - Configuration of MDDP applications.
 *
 * Copyright (C) 2018 MediaTek Inc.
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
#ifdef CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT
MDDP_MODULE_ID(MDDP_APP_TYPE_USB)
MDDP_MODULE_PREFIX(mddpu)
#endif

#ifdef CONFIG_MTK_MDDP_WH_SUPPORT
MDDP_MODULE_ID(MDDP_APP_TYPE_WH)
MDDP_MODULE_PREFIX(mddpwh)
#endif
