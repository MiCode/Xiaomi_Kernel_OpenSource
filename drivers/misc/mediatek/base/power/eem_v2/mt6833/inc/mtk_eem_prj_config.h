/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef _MTK_EEM_PRJ_CONFIG_H_
#define _MTK_EEM_PRJ_CONFIG_H_

/*
 * ##########################
 * debug log control
 * ############################
 */
#define CONFIG_EEM_SHOWLOG (0)
#define EN_ISR_LOG         (0)

/*
 * ##########################
 * efuse feature
 * ############################
 */
#define EEM_FAKE_EFUSE (0)
#define FAKE_SN_DVT_EFUSE_FOR_DE	(0)

/*
 * ##########################
 * SN feature
 * ############################
 */
/* dump SN data */
#define FULL_REG_DUMP_SNDATA		(0)
#define ENABLE_COUNT_SNTEMP			(1)

#define VMIN_PREDICT_ENABLE	(0)

/*
 * ##########################
 * phase out define
 * ############################
 */
#define SUPPORT_PI_LOG_AREA (0)


#endif

