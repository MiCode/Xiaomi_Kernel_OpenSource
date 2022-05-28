/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAM_DEBUG_OPTION_H
#define __MTK_CAM_DEBUG_OPTION_H

/*
 * To dubug format/crop related
 */
#define CAM_DEBUG_FORMAT	0
#define CAM_DEBUG_IPI_BUF	1

unsigned int cam_debug_opts(void);

static inline bool cam_debug_enabled(unsigned int type)
{
	return cam_debug_opts() & (1U << type);
}

#define CAM_DEBUG_ENABLED(type)		cam_debug_enabled(CAM_DEBUG_ ## type)

#endif /* __MTK_CAM_DEBUG_OPTION_H */
