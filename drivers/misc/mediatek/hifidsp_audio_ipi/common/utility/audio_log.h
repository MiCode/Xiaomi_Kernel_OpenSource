/*
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

#ifndef AUDIO_LOG_H
#define AUDIO_LOG_H

#include <linux/printk.h>

#ifndef AUD_LOG_V
#if 1
#define AUD_LOG_V(x...) pr_debug(x)
#else
#define AUD_LOG_V(x...)
#endif
#endif

#ifndef AUD_LOG_D
#define AUD_LOG_D pr_info
#endif

#ifndef AUD_LOG_W
#define AUD_LOG_W pr_info
#endif

#ifndef AUD_LOG_E
#define AUD_LOG_E pr_notice
#endif

#endif /* end of AUDIO_LOG_H */

