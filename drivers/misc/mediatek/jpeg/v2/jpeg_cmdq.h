/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __JPEG_CMDQ_H__
#define __JPEG_CMDQ_H__

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t cmdqJpegClockOn(uint64_t engineFlag);

int32_t cmdqJpegDumpInfo(uint64_t engineFlag,
			int level);

int32_t cmdqJpegResetEng(uint64_t engineFlag);

int32_t cmdqJpegClockOff(uint64_t engineFlag);

#ifdef __cplusplus
}
#endif

#endif  /* __CMDQ_MDP_H__ */
