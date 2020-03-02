/*
 * Copyright (C) 2018 MediaTek Inc.
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

