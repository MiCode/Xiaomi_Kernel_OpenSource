/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MDLA_DECODER_H__
#define __MDLA_DECODER_H__

#ifdef CONFIG_MTK_MDLA_DEBUG
void mdla_decode(const char *cmd, char *str, int size);
#else
static inline void mdla_decode(const char *cmd, char *str, int size)
{
}
#endif

#endif

