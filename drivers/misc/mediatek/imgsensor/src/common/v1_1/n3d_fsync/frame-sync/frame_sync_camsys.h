/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_CAMSYS_H
#define _FRAME_SYNC_CAMSYS_H


/*******************************************************************************
 * input:
 *     flag:"non 0" -> Start sync frame;
 *              "0" -> End sync frame;
 ******************************************************************************/
unsigned int fs_sync_frame(unsigned int flag);

#endif
